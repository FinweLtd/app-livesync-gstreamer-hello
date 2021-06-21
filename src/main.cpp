/*
 * Demo gstreamer app for negotiating and streaming a sendrecv webrtc stream
 * with a browser JS app.
 *
 * gcc webrtc-sendrecv.c $(pkg-config --cflags --libs gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0) -o webrtc-sendrecv
 *
 * Author: Nirbheek Chauhan <nirbheek@centricular.com>
 */

/*
 * Heavily modified by Finwe Ltd. to work with Finwe's Socket.IO based 
 * SignalingServer and a 360 camera running Finwe's LiveSYNC app as a 
 * video source, instead of a browser JS app. Converted from C to C++.
 * 
 * Author: Tapani Rantakokko <tapani.rantakokko@finwe.fi>
 */

#include <gst/gst.h>
#include <gst/sdp/sdp.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

/* For signalling */
#include "sio_client.h"
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <json-glib/json-glib.h>

#define HIGHLIGHT(__O__) std::cout << "\e[1;31m" << __O__ << "\e[0m" << std::endl

#include <string.h>

enum AppState
{
    APP_STATE_UNKNOWN = 0,
    APP_STATE_ERROR = 1, /* generic error */
    SERVER_CONNECTING = 1000,
    SERVER_CONNECTION_ERROR,
    SERVER_CONNECTED, /* Ready to register */
    SERVER_REGISTERING = 2000,
    SERVER_REGISTRATION_ERROR,
    SERVER_REGISTERED, /* Ready to call a peer */
    SERVER_CLOSED,     /* server connection closed by us or the server */
    PEER_CONNECTING = 3000,
    PEER_CONNECTION_ERROR,
    PEER_CONNECTED,
    PEER_CALL_NEGOTIATING = 4000,
    PEER_CALL_STARTED,
    PEER_CALL_STOPPING,
    PEER_CALL_STOPPED,
    PEER_CALL_ERROR,
};

static GMainLoop *loop;
static GstElement *pipe1, *webrtc1;
static GObject *send_channel, *receive_channel;

static enum AppState app_state = APP_STATE_UNKNOWN;
static const gchar *peer_id = nullptr;
static const gchar *server_url = nullptr;
static gboolean disable_ssl = FALSE;
static gboolean remote_is_offerer = FALSE;

static GOptionEntry entries[] = {
    {"server", 0, 0, G_OPTION_ARG_STRING, &server_url,
     "Signalling server to connect to", "URL"},
    {"disable-ssl", 0, 0, G_OPTION_ARG_NONE, &disable_ssl, "Disable ssl", nullptr},
    {"remote-offerer", 0, 0, G_OPTION_ARG_NONE, &remote_is_offerer,
     "Request that the peer generate the offer and we'll answer", nullptr},
    {nullptr},
};

using namespace std;
std::mutex _lock;
std::condition_variable_any _cond;
bool connect_finish = false;
sio::socket::ptr current_socket;
static sio::client h;

static gboolean cleanup_and_quit_loop(const gchar *msg, enum AppState state)
{
    if (msg)
        g_printerr("%s\n", msg);
    if (state > 0)
        app_state = state;

    h.close();
    h.clear_con_listeners();

    if (loop)
    {
        g_main_loop_quit(loop);
        loop = nullptr;
    }

    /* To allow usage as a GSourceFunc */
    return G_SOURCE_REMOVE;
}

/**
 * Listener for connection events to the signaling server.
 */
class connection_listener
{
    sio::client &handler;

public:
    connection_listener(sio::client &h) : handler(h)
    {
    }

    void on_connected()
    {
        _lock.lock();
        _cond.notify_all();
        connect_finish = true;
        _lock.unlock();
        app_state = SERVER_CONNECTED;
        g_print("Signaling connection opened\n");
    }
    void on_close(sio::client::close_reason const &reason)
    {
        g_print("Signaling connection closed, reason: %d\n", reason);
        //exit(0); //TODO error handling/reconnect logic, if needed
        // reason == 0: normal, reason == 1: drop

        app_state = SERVER_CLOSED;
        cleanup_and_quit_loop("Server connection closed", APP_STATE_UNKNOWN);
    }

    void on_fail()
    {
        g_printerr("Signaling connection failed\n");
        //exit(0); //TODO error handling/reconnect logic, if needed

        app_state = SERVER_CONNECTION_ERROR;
        cleanup_and_quit_loop("Server connection closed", APP_STATE_ERROR);
    }
};

static void response_to_init(sio::socket::ptr current_socket)
{
    // emit: {'sub':'Player-Gstreamer', 'role': 'receiver'}
    sio::message::ptr jsonObj = sio::object_message::create();
    jsonObj->get_map()["sub"] = sio::string_message::create("Player-Gstreamer");
    jsonObj->get_map()["role"] = sio::string_message::create("receiver");
    g_print("SEND: 'init', %s\n", "{'sub':'Player-Gstreamer', 'role': 'receiver'}");
    current_socket->emit("init", jsonObj, [&](sio::message::list const &msg)
                         {
                             g_print("ACK: ");
                             if (msg.size() > 0)
                             {
                                 sio::message::ptr ack = msg[0]; // assume 1 msg
                                 switch (ack->get_flag())
                                 {
                                 case sio::message::flag_boolean:
                                 {
                                     bool initialized = ack->get_bool();
                                     if (initialized)
                                     {
                                         g_print("initialization OK\n");
                                     }
                                     else
                                     {
                                         g_print("initialization FAILED\n");
                                     }
                                     break;
                                 }
                                 case sio::message::flag_integer:
                                 case sio::message::flag_double:
                                 case sio::message::flag_string:
                                 case sio::message::flag_binary:
                                 case sio::message::flag_array:
                                 case sio::message::flag_object:
                                 case sio::message::flag_null:
                                 default:
                                 {
                                     g_printerr("Unexpected response type from signaling server: %d", ack->get_flag());
                                     break;
                                 }
                                 }
                             }
                             else
                             {
                                 g_printerr("Unexpected response type from signaling server: empty ack\n");
                             }
                         });
}

/*
 * Connect to the signalling server. This is the entrypoint for everything else.
 */
static void connect_to_socketio_server_async(void)
{
    // The original webrtc-sendrecv example uses libsoup to connect to a
    // websocket server. With a LiveSYNC enabled 360 camera, we use Socket.IO
    // for signaling and thus replace libsoup with Socket.IO cpp client.

    // Setup connection listener.
    connection_listener l(h);
    h.set_open_listener(std::bind(&connection_listener::on_connected, &l));
    h.set_close_listener(std::bind(&connection_listener::on_close, &l, std::placeholders::_1));
    h.set_fail_listener(std::bind(&connection_listener::on_fail, &l));

    g_print("Connecting to SignalingServer %s\n", server_url);
    h.set_logs_verbose();
    h.connect(server_url);

    app_state = SERVER_CONNECTING;

    // Wait until connected / connection attempt failed.
    _lock.lock();
    if (!connect_finish)
    {
        _cond.wait(_lock);
    }
    _lock.unlock();

    // Open socket.
    current_socket = h.socket();

    // We use the default namespace '/'.
    g_print("Namespace: %s\n", current_socket->get_namespace().c_str());

    // Upon connect signaling server sends 'init' request and we must respond.
    current_socket->on("init", sio::socket::event_listener_aux([&](
                                                                   string const &name, sio::message::ptr const &data, bool isAck, sio::message::list &ack_resp)
                                                               {
                                                                   _lock.lock();

                                                                   bool response = false;
                                                                   if (app_state == SERVER_CONNECTED)
                                                                   {
                                                                       g_print("RECV: INIT -> registering...\n");
                                                                       app_state = SERVER_REGISTERING;
                                                                       response = true;
                                                                   }

                                                                   _cond.notify_all();
                                                                   _lock.unlock();

                                                                   current_socket->off("init");

                                                                   if (response)
                                                                   {
                                                                       response_to_init(current_socket);
                                                                   }
                                                               }));
}

/**
 * Check that required Gstreamer plugins are installed.
 */
static gboolean check_plugins(void)
{
    int i;
    gboolean ret;
    GstPlugin *plugin;
    GstRegistry *registry;
    const gchar *needed[] = {"opus", "vpx", "nice", "webrtc", "dtls", "srtp",
                             "rtpmanager", "videotestsrc", "audiotestsrc", NULL};

    registry = gst_registry_get();
    ret = TRUE;
    g_print("Checking required plugins...");
    for (i = 0; i < g_strv_length((gchar **)needed); i++)
    {
        plugin = gst_registry_find_plugin(registry, needed[i]);
        if (!plugin)
        {
            if (ret == TRUE)
            {
                g_print(" ERROR!\n");
            }
            g_print("Required gstreamer plugin '%s' not found\n", needed[i]);
            ret = FALSE;
            continue;
        }
        gst_object_unref(plugin);
    }

    if (ret == TRUE)
    {
        g_print(" OK\n");
    }

    return ret;
}

/**
 * main function - the program start from here.
 */
int main(int argc, char *argv[])
{
    g_print("*** LiveSYNC Gstreamer example ***\n");

    GOptionContext *context;
    GError *error = nullptr;

    context = g_option_context_new("- LiveSYNC GStreamer example");
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_add_group(context, gst_init_get_option_group());
    if (!g_option_context_parse(context, &argc, &argv, &error))
    {
        g_printerr("Error initializing: %s\n", error->message);
        return -1;
    }

    if (!check_plugins())
        return -1;

    g_print("Checking required parameters...");
    if (!server_url)
    {
        g_printerr(" ERROR!\n");
        g_printerr("--server is a required argument, for example:\n");
        g_printerr("--server https://192.168.1.100:443/rtc/socket.io\n");
        return -1;
    }
    else
    {
        g_printerr(" OK\n");
    }

    // Disable ssl when running a localhost server, because
    // it's probably a test server with a self-signed certificate
    {
        GstUri *uri = gst_uri_from_string(server_url);
        if (g_strcmp0("localhost", gst_uri_get_host(uri)) == 0 ||
            g_strcmp0("127.0.0.1", gst_uri_get_host(uri)) == 0)
            disable_ssl = TRUE;
        gst_uri_unref(uri);
    }

    loop = g_main_loop_new(nullptr, FALSE);

    connect_to_socketio_server_async();

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    if (pipe1)
    {
        gst_element_set_state(GST_ELEMENT(pipe1), GST_STATE_NULL);
        g_print("Pipeline stopped\n");
        gst_object_unref(pipe1);
    }

    return 0;
}
