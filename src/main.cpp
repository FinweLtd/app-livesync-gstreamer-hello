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

#include <string.h>

enum AppState
{
  APP_STATE_UNKNOWN = 0,
  APP_STATE_ERROR = 1,          /* generic error */
  SERVER_CONNECTING = 1000,
  SERVER_CONNECTION_ERROR,
  SERVER_CONNECTED,             /* Ready to register */
  SERVER_REGISTERING = 2000,
  SERVER_REGISTRATION_ERROR,
  SERVER_REGISTERED,            /* Ready to call a peer */
  SERVER_CLOSED,                /* server connection closed by us or the server */
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
static const gchar *peer_id = NULL;
static const gchar *server_url = NULL;
static gboolean disable_ssl = FALSE;
static gboolean remote_is_offerer = FALSE;

static GOptionEntry entries[] = {
    {"server", 0, 0, G_OPTION_ARG_STRING, &server_url,
     "Signalling server to connect to", "URL"},
    {"disable-ssl", 0, 0, G_OPTION_ARG_NONE, &disable_ssl, "Disable ssl", NULL},
    {"remote-offerer", 0, 0, G_OPTION_ARG_NONE, &remote_is_offerer,
     "Request that the peer generate the offer and we'll answer", NULL},
    {NULL},
};

/*
 * Connect to the signalling server. This is the entrypoint for everything else.
 */
static void
connect_to_socketio_server_async(void)
{
    // The origianl webrtc-sendrecv example uses libsoup to connect to a
    // websocket server. With a LiveSYNC enabled 360 camera, we use Socket.IO
    // for signaling and thus replace libsoup with Socket.IO cpp client.

    sio::client h;
    h.connect(server_url);
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
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Error initializing: %s\n", error->message);
        return -1;
    }

    if (!check_plugins())
        return -1;

    g_print("Checking required parameters...");
    if (!server_url) {
        g_printerr(" ERROR!\n");
        g_printerr("--server is a required argument, for example:\n");
        g_printerr("--server https://192.168.1.100:443/rtc/socket.io\n");
        return -1;
    } else {
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
