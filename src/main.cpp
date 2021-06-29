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
#include <cstring>
#include <regex>

enum AppState
{
    APP_STATE_UNKNOWN = 0,
    APP_STATE_INITIALIZING = 1,
    APP_STATE_ERROR = 2, /* generic error */
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
static const gchar *own_id = "LiveSYNC Gstreamer";
static const gchar *peer_id = nullptr;
static const gchar *server_url = nullptr;
static gboolean disable_ssl = FALSE;
static gboolean remote_is_offerer = FALSE;
static gboolean camera_free = FALSE;

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
static sio::client client;

#define STUN_SERVER " stun-server=stun://stun.l.google.com:19302 "
#define RTP_CAPS_OPUS "application/x-rtp,media=audio,encoding-name=OPUS,payload="
#define RTP_CAPS_VP8 "application/x-rtp,media=video,encoding-name=VP8,payload="
#define RTP_PAYLOAD_TYPE "96"

/**
 * Handle cleanup and quit running the app.
 */
static gboolean cleanup_and_quit_loop(const gchar *msg, enum AppState state)
{
    // Notify user and set app final state.
    if (msg)
        g_printerr("Quitting the app, reason: %s\n", msg);
    if (state > 0)
        app_state = state;

    // Close connection to the signal server and remove connection listener.
    client.close();
    client.clear_con_listeners();

    // Stop the main loop.
    if (loop)
    {
        g_main_loop_quit(loop);
        loop = nullptr;
    }

    // Cleanup.
    if (peer_id)
    {
        delete peer_id;
        peer_id = nullptr;
    }

    // To allow usage as a GSourceFunc.
    return G_SOURCE_REMOVE;
}

/**
 * Convert JSON object to string.
 */
static gchar *get_string_from_json_object(JsonObject *object)
{
    JsonNode *root;
    JsonGenerator *generator;
    gchar *text;

    /* Make it the root node */
    root = json_node_init_object(json_node_alloc(), object);
    generator = json_generator_new();
    json_generator_set_root(generator, root);
    text = json_generator_to_data(generator, NULL);

    /* Release everything */
    g_object_unref(generator);
    json_node_free(root);
    return text;
}

/**
 * Convert a JSON string into a JSON node (root) for reading.
 */
static JsonNode *get_json_node_from_string(JsonParser *parser,
                                           const string data)
{
    GError *err = nullptr;
    if (!json_parser_load_from_data(parser, data.c_str(), -1, &err))
    {
        g_printerr("Error in parsing JSON string %s: %s",
                   data.c_str(), err->message);
        g_error_free(err);
        return nullptr;
    }
    else
    {
        // Debug print parsed JSON fields?
        if (false)
        {
            JsonReader *reader = json_reader_new(json_parser_get_root(parser));
            char **members = json_reader_list_members(reader);
            int i = 0;
            while (members[i] != 0)
            {
                std::string m = members[i];
                g_print("Found JSON key: %s\n", m.c_str());
                /*
                // For parsing a value, we need to know its data type. Example:
                if (m == "key1")
                {
                    json_reader_read_member(reader, members[i]);
                    std::string value = json_reader_get_string_value(reader);
                    json_reader_end_member(reader);
                    printf("parse member %s\n", members[i]);
                    printf("parse value %s\n", value.c_str());
                }*/
                i++;
            }
            g_strfreev(members);
            g_object_unref(reader);
        }

        return json_parser_get_root(parser);
    }
}

/**
 * Print help to console.
 */
static void print_help()
{
    g_print("Type a command and hit ENTER to control the camera:\n");
    g_print("===================================================\n");
    g_print("help = print this message\n");
    g_print("equi = switch camera to equirectangular projection\n");
    g_print("rect = switch camera to rectilinear projection\n");
    g_print("exit = exit from video call and quit the program\n");
    g_print("===================================================\n");
}

/**
 * Simple prompt.
 */
static void prompt()
{
    g_print("LiveSYNC> ");
}

/**
 * Response to user input.
 */
static void handlecommand(gchar *sz)
{
    gchar *text;
    JsonObject *ice, *msg;
    string op;
    string type;
    double value;
    double x;
    double y;

    if (strcmp(sz, "help\n") == 0)
    {
        print_help();
    }
    else if (strcmp(sz, "equi\n") == 0)
    {
        op = "projection";
        type = "equirectangular";
    }
    else if (strcmp(sz, "rect\n") == 0)
    {
        op = "projection";
        type = "rectilinear";
    }
    else if (strcmp(sz, "up\n") == 0)
    {
        op = "up";
    }
    else if (strcmp(sz, "down\n") == 0)
    {
        op = "down";
    }
    else if (strcmp(sz, "left\n") == 0)
    {
        op = "left";
    }
    else if (strcmp(sz, "right\n") == 0)
    {
        op = "right";
    }
    else if (strcmp(sz, "zoom-in\n") == 0)
    {
        op = "zoom-in";
    }
    else if (strcmp(sz, "zoom-out\n") == 0)
    {
        op = "zoom-out";
    }
    else if (strcmp(sz, "zoom-delta\n") == 0)
    {
        op = "zoom-delta";
        value = -4.000244140625;
    }
    else if (strcmp(sz, "pan-vector\n") == 0)
    {
        op = "pan-vector";
        x = 0.0028697826244212963;
        y = 0.17291244430158606;
    }
    else if (strcmp(sz, "pan-tilt\n") == 0)
    {
        op = "pan-tilt";
        x = 0.011101582502542789;
        y = -0.0024806650439698494;
    }
    else if (strcmp(sz, "exit\n") == 0)
    {
        cleanup_and_quit_loop("User chose to exit the app.",
                              APP_STATE_UNKNOWN);
    }
    else
    {
        g_print("\nUnknown command: %s\n", sz);
        print_help();
    }

    if (!op.empty())
    {
        msg = json_object_new();
        json_object_set_string_member(msg, "target", peer_id);
        json_object_set_string_member(msg, "source", own_id);
        json_object_set_string_member(msg, "op", op.c_str());
        if (!type.empty())
        {
            json_object_set_string_member(msg, "type", type.c_str());
        }
        if (value != 0)
        {
            json_object_set_double_member(msg, "value", value);
        }
        if (x != 0)
        {
            json_object_set_double_member(msg, "x", x);
        }
        if (y != 0)
        {
            json_object_set_double_member(msg, "y", y);
        }
        text = get_string_from_json_object(msg);
        json_object_unref(msg);
        g_print("SEND: 'message', %s\n", text);
        current_socket->emit(
            "message", (std::string)text, [&](sio::message::list const &msg)
            {
                // Prevent flooding the log.
                //g_print("ACK:  'new-ice-candidate', \n");
            });
        g_free(text);
    }

    prompt();
}

/**
 * Listen for user's commands from keyboard, and control camera accordingly.
 */
static gboolean mycallback(GIOChannel *channel, GIOCondition cond, gpointer data)
{
    gchar *str_return;
    gsize length;
    gsize terminator_pos;
    GError *error = NULL;

    if (g_io_channel_read_line(channel, &str_return, &length, &terminator_pos, &error) == G_IO_STATUS_ERROR)
    {
        g_warning("Something went wrong");
    }
    if (error != NULL)
    {
        g_printerr("Error: %s", error->message);
        exit(1);
    }

    handlecommand(str_return);

    g_free(str_return);
    return TRUE;
}

/**
 * Called when we need to handle a media stream.
 */
static void handle_media_stream(GstPad *pad, GstElement *pipe,
                                const char *convert_name, const char *sink_name)
{
    GstPad *qpad;
    GstElement *q, *conv, *resample, *sink;
    GstPadLinkReturn ret;

    g_print("Trying to handle stream with %s ! %s\n", convert_name, sink_name);

    q = gst_element_factory_make("queue", NULL);
    g_assert_nonnull(q);
    conv = gst_element_factory_make(convert_name, NULL);
    g_assert_nonnull(conv);
    sink = gst_element_factory_make(sink_name, NULL);
    g_assert_nonnull(sink);

    if (g_strcmp0(convert_name, "audioconvert") == 0)
    {
        /* Might also need to resample, so add it just in case.
     * Will be a no-op if it's not required. */
        resample = gst_element_factory_make("audioresample", NULL);
        g_assert_nonnull(resample);
        gst_bin_add_many(GST_BIN(pipe), q, conv, resample, sink, NULL);
        gst_element_sync_state_with_parent(q);
        gst_element_sync_state_with_parent(conv);
        gst_element_sync_state_with_parent(resample);
        gst_element_sync_state_with_parent(sink);
        gst_element_link_many(q, conv, resample, sink, NULL);
    }
    else
    {
        gst_bin_add_many(GST_BIN(pipe), q, conv, sink, NULL);
        gst_element_sync_state_with_parent(q);
        gst_element_sync_state_with_parent(conv);
        gst_element_sync_state_with_parent(sink);
        gst_element_link_many(q, conv, sink, NULL);
    }

    qpad = gst_element_get_static_pad(q, "sink");

    ret = gst_pad_link(pad, qpad);
    g_assert_cmphex(ret, ==, GST_PAD_LINK_OK);

    g_print("\n*** We are LIVE and video stream from remote camera should be visible on screen! ***\n");

    print_help();
    prompt();
}

/**
 * Called when we get an incoming stream (video/audio).
 */
static void on_incoming_decodebin_stream(GstElement *decodebin, GstPad *pad,
                                         GstElement *pipe)
{
    GstCaps *caps;
    const gchar *name;

    if (!gst_pad_has_current_caps(pad))
    {
        g_printerr("Pad '%s' has no caps, can't do anything, ignoring\n",
                   GST_PAD_NAME(pad));
        return;
    }

    caps = gst_pad_get_current_caps(pad);
    name = gst_structure_get_name(gst_caps_get_structure(caps, 0));

    if (g_str_has_prefix(name, "video"))
    {
        handle_media_stream(pad, pipe, "videoconvert", "autovideosink");
    }
    else if (g_str_has_prefix(name, "audio"))
    {
        handle_media_stream(pad, pipe, "audioconvert", "autoaudiosink");
    }
    else
    {
        g_printerr("Unknown pad %s, ignoring", GST_PAD_NAME(pad));
    }
}

/**
 * Called when we get an incoming stream.
 */
static void on_incoming_stream(GstElement *webrtc, GstPad *pad, GstElement *pipe)
{
    GstElement *decodebin;
    GstPad *sinkpad;

    g_print("-> Incoming stream\n");

    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
        return;

    decodebin = gst_element_factory_make("decodebin", NULL);
    g_signal_connect(decodebin, "pad-added",
                     G_CALLBACK(on_incoming_decodebin_stream), pipe);
    gst_bin_add(GST_BIN(pipe), decodebin);
    gst_element_sync_state_with_parent(decodebin);

    sinkpad = gst_element_get_static_pad(decodebin, "sink");
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}

/**
 * Send ICE candidate to peer (camera).
 */
static void send_ice_candidate_message(GstElement *webrtc G_GNUC_UNUSED,
                                       guint mlineindex,
                                       gchar *candidate,
                                       gpointer user_data G_GNUC_UNUSED)
{
    gchar *text;
    JsonObject *ice, *msg;

    if (app_state < PEER_CALL_NEGOTIATING)
    {
        cleanup_and_quit_loop("Can't send ICE, not in a call!", APP_STATE_ERROR);
        return;
    }

    ice = json_object_new();
    json_object_set_string_member(ice, "candidate", candidate);
    json_object_set_int_member(ice, "sdpMid", 0);
    json_object_set_int_member(ice, "sdpMLineIndex", mlineindex);

    msg = json_object_new();
    json_object_set_string_member(msg, "target", peer_id);
    json_object_set_string_member(msg, "source", own_id);
    json_object_set_string_member(msg, "type", "new-ice-candidate");
    json_object_set_object_member(msg, "candidate", ice);

    text = get_string_from_json_object(msg);
    json_object_unref(msg);

    g_print("SEND: 'new-ice-candidate', %s\n", text);
    current_socket->emit(
        "new-ice-candidate", (std::string)text, [&](sio::message::list const &msg)
        {
            // Prevent flooding the log.
            //g_print("ACK:  'new-ice-candidate', \n");
        });

    g_free(text);
}

/**
 * Send video offer to peer (camera).
 */
static void send_sdp_to_peer(GstWebRTCSessionDescription *desc)
{
    gchar *text;
    JsonObject *msg, *sdp;

    if (app_state < PEER_CALL_NEGOTIATING)
    {
        cleanup_and_quit_loop("Can't send SDP to peer, not in a call",
                              APP_STATE_ERROR);
        return;
    }

    text = gst_sdp_message_as_text(desc->sdp);

    // Commented out; this was fixed from the camera's end (bundle policy).
    // Miserable hack to add bundle specifier (why it isn't there?)
    //string tmp = string(gst_sdp_message_as_text(desc->sdp));
    //string source = "t=0 0\r\n";
    //string target = "a=group:BUNDLE 0 1\r\n";
    //tmp.insert(tmp.find(source) + source.size(), target);

    //string source2 = "a=sendrecv";
    //string target2 = "a=mid:0\r\n";
    //tmp.insert(tmp.find(source2), target2);

    //text = new char[tmp.size() + 1];
    //std::strcpy(text, tmp.c_str());

    sdp = json_object_new();
    if (desc->type == GST_WEBRTC_SDP_TYPE_OFFER)
    {
        g_print("Sending offer:\n%s\n", text);
        json_object_set_string_member(sdp, "type", "offer");
    }
    else if (desc->type == GST_WEBRTC_SDP_TYPE_ANSWER)
    {
        g_print("Sending answer:\n%s\n", text);
        json_object_set_string_member(sdp, "type", "answer");
    }
    else
    {
        g_assert_not_reached();
    }

    json_object_set_string_member(sdp, "sdp", text);
    g_free(text);

    msg = json_object_new();
    json_object_set_string_member(msg, "target", peer_id);
    json_object_set_object_member(msg, "sdp", sdp);

    text = get_string_from_json_object(msg);
    json_object_unref(msg);

    g_print("SEND: 'video-offer', %s\n", text);
    current_socket->emit(
        "video-offer", (std::string)text, [&](sio::message::list const &msg)
        {
            // Prevent flooding the log.
            //g_print("ACK:  'video-offer', \n");
        });

    g_free(text);
}

/**
 * Offer created by our pipeline, to be sent to the peer (camera).
 */
static void on_offer_created(GstPromise *promise, gpointer user_data)
{
    GstWebRTCSessionDescription *offer = NULL;
    const GstStructure *reply;

    g_assert_cmphex(app_state, ==, PEER_CALL_NEGOTIATING);

    g_assert_cmphex(gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
    reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "offer",
                      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
    gst_promise_unref(promise);

    promise = gst_promise_new();
    g_signal_emit_by_name(webrtc1, "set-local-description", offer, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);

    // Send offer to peer (camera).
    send_sdp_to_peer(offer);
    gst_webrtc_session_description_free(offer);
}

/**
 * Called when WebRTC needs to negotiate with the peer.
 */
static void on_negotiation_needed(GstElement *element, gpointer user_data)
{
    app_state = PEER_CALL_NEGOTIATING;

    if (remote_is_offerer)
    {
        g_print("NOT SUPPORTED. Currently, the receiver creates the video offer.\n");
    }
    else
    {
        GstPromise *promise;
        promise =
            gst_promise_new_with_change_func(on_offer_created, user_data, NULL);
        ;
        g_signal_emit_by_name(webrtc1, "create-offer", NULL, promise);
    }
}

/**
 * Called when there has been an error on data channel.
 */
static void data_channel_on_error(GObject *dc, gpointer user_data)
{
    cleanup_and_quit_loop("Data channel error", APP_STATE_UNKNOWN);
}

/**
 * Called when data channel has been opened.
 */
static void data_channel_on_open(GObject *dc, gpointer user_data)
{
    GBytes *bytes = g_bytes_new("data", strlen("data"));
    g_print("data channel opened\n");
    g_signal_emit_by_name(dc, "send-string", "Hi! from GStreamer");
    g_signal_emit_by_name(dc, "send-data", bytes);
    g_bytes_unref(bytes);
}

/**
 * Called when data channel has been closed.
 */
static void data_channel_on_close(GObject *dc, gpointer user_data)
{
    cleanup_and_quit_loop("Data channel closed", APP_STATE_UNKNOWN);
}

/**
 * Called when we receive a message via the data channel. 
 */
static void data_channel_on_message_string(GObject *dc, gchar *str, gpointer user_data)
{
    g_print("Received data channel message: %s\n", str);
}

/**
 * Connect data channel signals.
 */
static void connect_data_channel_signals(GObject *data_channel)
{
    g_signal_connect(data_channel, "on-error",
                     G_CALLBACK(data_channel_on_error), NULL);
    g_signal_connect(data_channel, "on-open", G_CALLBACK(data_channel_on_open),
                     NULL);
    g_signal_connect(data_channel, "on-close",
                     G_CALLBACK(data_channel_on_close), NULL);
    g_signal_connect(data_channel, "on-message-string",
                     G_CALLBACK(data_channel_on_message_string), NULL);
}

/**
 * Called when we get a data channel.
 */
static void on_data_channel(GstElement *webrtc, GObject *data_channel,
                            gpointer user_data)
{
    connect_data_channel_signals(data_channel);
    receive_channel = data_channel;
}

/**
 * Notify in which ICE gathering state we currently are.
 */
static void on_ice_gathering_state_notify(GstElement *webrtcbin,
                                          GParamSpec *pspec,
                                          gpointer user_data)
{
    GstWebRTCICEGatheringState ice_gather_state;
    const gchar *new_state = "unknown";

    g_object_get(webrtcbin, "ice-gathering-state", &ice_gather_state, NULL);
    switch (ice_gather_state)
    {
    case GST_WEBRTC_ICE_GATHERING_STATE_NEW:
        new_state = "new";
        break;
    case GST_WEBRTC_ICE_GATHERING_STATE_GATHERING:
        new_state = "gathering";
        break;
    case GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE:
        new_state = "complete";
        break;
    }
    g_print("ICE gathering state changed to %s\n", new_state);
}

/**
 * Start WebRTC pipeline and try open streams with the other end.
 */
static gboolean start_pipeline(void)
{
    GstStateChangeReturn ret;
    GError *error = NULL;
    GstCaps *video_caps;
    GstWebRTCRTPTransceiver *trans = NULL;

    pipe1 =
        /*
        gst_parse_launch("webrtcbin name=sendrecv stun-server=stun://" STUN_SERVER " "
                         //" ! rtpvp8depay ! vp8dec ! videoconvert ! queue ! fakevideosink "
                         //"videotestsrc is-live=true pattern=ball ! videoconvert ! queue ! vp8enc deadline=1 ! rtpvp8pay ! "
                         //"queue ! " RTP_CAPS_VP8 "96 ! sendrecv. "
                         , &error);
    */
        /*
        gst_parse_launch("webrtcbin name=sendrecv stun-server=stun://" STUN_SERVER " "
                         "audiotestsrc is-live=true wave=red-noise ! audioconvert ! audioresample ! queue ! opusenc ! rtpopuspay ! "
                         "queue ! " RTP_CAPS_OPUS "97 ! sendrecv. ",
                         &error);
    */

        // bundle-policy=max-bundle
        gst_parse_launch("webrtcbin name=sendrecv bundle-policy=max-compat " STUN_SERVER
                         "videotestsrc is-live=true pattern=ball ! videoconvert ! queue ! vp8enc deadline=1 ! rtpvp8pay ! "
                         "queue ! " RTP_CAPS_VP8 "96 ! sendrecv. "
                         //                   "audiotestsrc is-live=true wave=red-noise ! audioconvert ! audioresample ! queue ! opusenc ! rtpopuspay ! "
                         //                   "queue ! " RTP_CAPS_OPUS "97 ! sendrecv. "
                         ,
                         &error);

    if (error)
    {
        g_printerr("Failed to parse launch: %s\n", error->message);
        g_error_free(error);
        goto err;
    }

    webrtc1 = gst_bin_get_by_name(GST_BIN(pipe1), "sendrecv");
    g_assert_nonnull(webrtc1);

    /* This is the gstwebrtc entry point where we create the offer and so on. It
     * will be called when the pipeline goes to PLAYING. */
    g_signal_connect(webrtc1, "on-negotiation-needed",
                     G_CALLBACK(on_negotiation_needed), NULL);
    /* We need to transmit this ICE candidate to the camera via the Socket.IO
     * signalling server. Incoming ice candidates from the camera need to be
     * added by us too, see on_server_message() */
    g_signal_connect(webrtc1, "on-ice-candidate",
                     G_CALLBACK(send_ice_candidate_message), NULL);
    g_signal_connect(webrtc1, "notify::ice-gathering-state",
                     G_CALLBACK(on_ice_gathering_state_notify), NULL);

    gst_element_set_state(pipe1, GST_STATE_READY);

    // Below: commented out; we don't currently use data channels with LiveSYNC.
    /*
    g_signal_emit_by_name(webrtc1, "create-data-channel", "channel", NULL,
                          &send_channel);
    if (send_channel)
    {
        g_print("Created data channel\n");
        connect_data_channel_signals(send_channel);
    }
    else
    {
        g_print("Could not create data channel, is usrsctp available?\n");
    }

    g_signal_connect(webrtc1, "on-data-channel", G_CALLBACK(on_data_channel),
                     NULL);
    */

    // From sendonly/webrtc-recvonly-h264.c:
    // Create a 2nd transceiver for the receive only video stream.
    /*
    video_caps = gst_caps_from_string("application/x-rtp,media=video,encoding-name=H264,payload=" RTP_PAYLOAD_TYPE ",clock-rate=90000,packetization-mode=(string)1, profile-level-id=(string)42c016");
    g_signal_emit_by_name(webrtc1, "add-transceiver", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, video_caps, &trans);
    gst_caps_unref(video_caps);
    gst_object_unref(trans);
    */

    /* Incoming streams will be exposed via this signal */
    g_signal_connect(webrtc1, "pad-added", G_CALLBACK(on_incoming_stream),
                     pipe1);

    /* Lifetime is the same as the pipeline itself */
    gst_object_unref(webrtc1);

    g_print("Starting Gstreamer pipeline\n");
    ret = gst_element_set_state(GST_ELEMENT(pipe1), GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
        goto err;

    return TRUE;

err:
    if (pipe1)
        g_clear_object(&pipe1);
    if (webrtc1)
        webrtc1 = NULL;
    return FALSE;
}

/**
 * Check camera's incoming ICE candidate, and add or reject it.
 */
static gboolean add_ice_candidate(const gchar *text)
{
    g_print("Checking ICE candidate from %s...\n", peer_id);
    g_print("'new-ice-candidate' message=%s\n", text);

    JsonNode *root;
    JsonObject *object, *child;
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, text, -1, NULL))
    {
        g_printerr("Unknown message '%s', ignoring", text);
        g_object_unref(parser);
        return FALSE;
    }

    root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root))
    {
        g_printerr("Unknown json message '%s', ignoring", text);
        g_object_unref(parser);
        return FALSE;
    }

    object = json_node_get_object(root);
    if (json_object_has_member(object, "candidate"))
    {
        const gchar *candidate;
        gint sdpmlineindex;

        child = json_object_get_object_member(object, "candidate");
        candidate = json_object_get_string_member(child, "candidate");
        sdpmlineindex = json_object_get_int_member(child, "sdpMLineIndex");

        // Add ice candidate sent by remote peer.
        g_signal_emit_by_name(webrtc1, "add-ice-candidate", sdpmlineindex,
                              candidate);

        return TRUE;
    }
    else
    {
        g_printerr("Ignoring unknown JSON message:\n%s\n", text);
    }
    g_object_unref(parser);

    return FALSE;
}

/**
 * Check camera's answer to our video offer, and accept or reject call.
 */
static gboolean accept_call(const gchar *text)
{
    g_print("Checking video answer from %s...\n", peer_id);
    g_print("'video-answer' message=%s\n", text);

    JsonNode *root;
    JsonObject *object, *child;
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, text, -1, NULL))
    {
        g_printerr("Unknown message '%s', ignoring", text);
        g_object_unref(parser);
        return FALSE;
    }

    root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root))
    {
        g_printerr("Unknown json message '%s', ignoring", text);
        g_object_unref(parser);
        return FALSE;
    }

    object = json_node_get_object(root);
    if (json_object_has_member(object, "sdp"))
    {
        int ret;
        GstSDPMessage *sdp;
        const gchar *text, *sdptype;
        GstWebRTCSessionDescription *answer;

        g_assert_cmphex(app_state, ==, PEER_CALL_NEGOTIATING);

        child = json_object_get_object_member(object, "sdp");

        if (!json_object_has_member(child, "type"))
        {
            cleanup_and_quit_loop("ERROR: received SDP without 'type'",
                                  PEER_CALL_ERROR);
            return FALSE;
        }

        sdptype = json_object_get_string_member(child, "type");

        text = json_object_get_string_member(child, "sdp");
        ret = gst_sdp_message_new(&sdp);
        g_assert_cmphex(ret, ==, GST_SDP_OK);
        ret = gst_sdp_message_parse_buffer((guint8 *)text, strlen(text), sdp);
        g_assert_cmphex(ret, ==, GST_SDP_OK);

        if (g_str_equal(sdptype, "answer"))
        {
            g_print("Parsed SDP from 'video-answer':\n%s\n", text);
            answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER,
                                                        sdp);
            g_assert_nonnull(answer);

            // Set remote description on our pipeline.
            {
                GstPromise *promise = gst_promise_new();
                g_signal_emit_by_name(webrtc1, "set-remote-description", answer,
                                      promise);
                gst_promise_interrupt(promise);
                gst_promise_unref(promise);
            }
            app_state = PEER_CALL_STARTED;
            return TRUE;
        }
        else
        {
            g_printerr("Expected answer but received offer:\n%s\n", text);
        }
    }
    else
    {
        g_printerr("Ignoring unknown JSON message:\n%s\n", text);
    }
    g_object_unref(parser);

    return FALSE;
}

/**
 * Try to call to the camera device.
 */
static gboolean setup_call(const gchar *callTarget)
{
    g_print("Trying to call to %s ...\n", callTarget);

    app_state = PEER_CONNECTING;

    peer_id = g_strdup(callTarget);

    // Note: unlike webrtc-sendrecv example, we don't have any mechanism in
    // place for reserving a peer for an upcoming call via the SignalServer
    // i.e. we don't tell the camera in advance that we are about to call it.
    // Instead, we simply try to open the connection via WebRTC APIs.

    // In case of multiple clients competing for the same camera resource, we
    // could add a call-response system for reserving the camera in our use.

    app_state = PEER_CONNECTED;

    // Start negotiation (exchange SDP and ICE candidates).
    if (!start_pipeline())
        cleanup_and_quit_loop("ERROR: failed to start pipeline",
                              PEER_CALL_ERROR);

    return TRUE;
}

/**
 * Listener for connection events related to the signaling server.
 */
class connection_listener
{
    sio::client &handler;

public:
    connection_listener(sio::client &h) : handler(client) {}

    void on_connected()
    {
        _lock.lock();
        _cond.notify_all();
        connect_finish = true;
        _lock.unlock();
        app_state = SERVER_CONNECTED;
        g_print("Successfully connected to SignalingServer\n");
    }
    void on_close(sio::client::close_reason const &reason)
    {
        g_print("\nConnection to SignalingServer closed, reason: %d\n", reason);
        // reason: 0=normal, 1=drop

        //TODO Error handling/reconnect logic if needed (or use auto reconnect).

        app_state = SERVER_CLOSED;
        cleanup_and_quit_loop("Server connection closed", APP_STATE_UNKNOWN);
    }

    void on_fail()
    {
        g_printerr("Connection to SignalingServer failed (is it running?)\n");

        app_state = SERVER_CONNECTION_ERROR;
        cleanup_and_quit_loop("Server connection failed", APP_STATE_ERROR);
    }
};

/**
 * Bind to known signals and handle them.
 */
void bind_events()
{
    // When a camera device has successfully connected to the SignalServer.
    current_socket->on("device-authenticated", sio::socket::event_listener_aux(
                                                   [&](string const &name, sio::message::ptr const &data,
                                                       bool isAck, sio::message::list &ack_resp)
                                                   {
                                                       _lock.lock();
                                                       if (data->get_flag() == sio::message::flag_string)
                                                       {
                                                           g_print("RECV: 'device-authenticated', %s\n",
                                                                   data->get_string().c_str());
                                                       }
                                                       else
                                                       {
                                                           g_printerr("RECV: invalid data, check API!\n");
                                                       }
                                                       _lock.unlock();
                                                   }));

    // When a camera device is ready for a call.
    current_socket->on("device-ready", sio::socket::event_listener_aux(
                                           [&](string const &name, sio::message::ptr const &data,
                                               bool isAck, sio::message::list &ack_resp)
                                           {
                                               _lock.lock();
                                               if (data->get_flag() == sio::message::flag_string)
                                               {
                                                   g_print("RECV: 'device-ready', %s\n",
                                                           data->get_string().c_str());

                                                   if (camera_free)
                                                   {
                                                       if (!setup_call(data->get_string().c_str()))
                                                       {
                                                           cleanup_and_quit_loop("ERROR: Failed to setup call!",
                                                                                 PEER_CALL_ERROR);
                                                       }
                                                   }
                                                   else
                                                   {
                                                       g_print("Camera is ready but it is not free; not calling!");
                                                   }
                                               }
                                               else
                                               {
                                                   g_printerr("RECV: invalid data, check API!\n");
                                               }
                                               _lock.unlock();
                                           }));

    // When a camera device has disconnected from the SignalServer.
    current_socket->on("device-disconnected", sio::socket::event_listener_aux(
                                                  [&](string const &name, sio::message::ptr const &data,
                                                      bool isAck, sio::message::list &ack_resp)
                                                  {
                                                      _lock.lock();
                                                      if (data->get_flag() == sio::message::flag_string)
                                                      {
                                                          g_print("RECV: 'device-disconnected', %s\n",
                                                                  data->get_string().c_str());
                                                      }
                                                      else
                                                      {
                                                          g_printerr("RECV: invalid data, check API!\n");
                                                      }

                                                      _lock.unlock();
                                                  }));

    // Number of clients the camera device supports and currently has.
    current_socket->on("client-count", sio::socket::event_listener_aux(
                                           [&](string const &name, sio::message::ptr const &data,
                                               bool isAck, sio::message::list &ack_resp)
                                           {
                                               _lock.lock();
                                               if (data->get_flag() == sio::message::flag_string)
                                               {
                                                   //g_print("RECV: 'client-count', %s\n",
                                                   //        data->get_string().c_str());
                                                   JsonParser *parser = json_parser_new();
                                                   JsonNode *root = get_json_node_from_string(parser,
                                                                                              data->get_string());
                                                   JsonReader *reader = json_reader_new(root);
                                                   json_reader_read_member(reader, "connected-clients");
                                                   int conClients = json_reader_get_int_value(reader);
                                                   json_reader_end_member(reader);
                                                   json_reader_read_member(reader, "max-connected-clients");
                                                   int maxConClients = json_reader_get_int_value(reader);
                                                   json_reader_end_member(reader);
                                                   json_reader_read_member(reader, "streaming-clients");
                                                   int strClients = json_reader_get_int_value(reader);
                                                   json_reader_end_member(reader);
                                                   json_reader_read_member(reader, "max-streaming-clients");
                                                   int maxStrClients = json_reader_get_int_value(reader);
                                                   json_reader_end_member(reader);
                                                   g_object_unref(reader);
                                                   g_object_unref(parser);
                                                   g_print("RECV: 'client-count', connected %d/%d, streaming %d/%d\n",
                                                           conClients, maxConClients, strClients, maxStrClients);

                                                   if (conClients < maxConClients && strClients < maxStrClients)
                                                   {
                                                       g_print("Camera is currently free, we can try to call it.\n");
                                                       camera_free = TRUE;
                                                   }
                                                   else
                                                   {
                                                       g_print("Camera is currently reserved.\n");
                                                       camera_free = FALSE;
                                                   }
                                               }
                                               else
                                               {
                                                   g_printerr("RECV: invalid data, check API!\n");
                                               }
                                               _lock.unlock();
                                           }));

    // The type of video format (projection) currently active on camera device.
    current_socket->on("video-format", sio::socket::event_listener_aux(
                                           [&](string const &name, sio::message::ptr const &data,
                                               bool isAck, sio::message::list &ack_resp)
                                           {
                                               _lock.lock();
                                               if (data->get_flag() == sio::message::flag_string)
                                               {
                                                   JsonParser *parser = json_parser_new();
                                                   JsonNode *root = get_json_node_from_string(parser,
                                                                                              data->get_string());
                                                   JsonReader *reader = json_reader_new(root);
                                                   json_reader_read_member(reader, "projection");
                                                   string projection = json_reader_get_string_value(reader);
                                                   json_reader_end_member(reader);
                                                   g_object_unref(reader);
                                                   g_object_unref(parser);
                                                   g_print("RECV: 'video-format', projection=%s\n",
                                                           projection.c_str());
                                               }
                                               else
                                               {
                                                   g_printerr("RECV: invalid data, check API!\n");
                                               }

                                               _lock.unlock();
                                           }));

    current_socket->on("ready", sio::socket::event_listener_aux(
                                    [&](string const &name, sio::message::ptr const &data,
                                        bool isAck, sio::message::list &ack_resp)
                                    {
                                        _lock.lock();
                                        g_print("RECV: 'ready' -> \n");
                                        _lock.unlock();
                                    }));

    current_socket->on("video-offer", sio::socket::event_listener_aux(
                                          [&](string const &name, sio::message::ptr const &data,
                                              bool isAck, sio::message::list &ack_resp)
                                          {
                                              _lock.lock();
                                              g_print("RECV: 'video-offer' -> \n");
                                              _lock.unlock();
                                          }));

    current_socket->on("video-answer", sio::socket::event_listener_aux(
                                           [&](string const &name, sio::message::ptr const &data,
                                               bool isAck, sio::message::list &ack_resp)
                                           {
                                               _lock.lock();
                                               if (app_state == PEER_CALL_NEGOTIATING)
                                               {
                                                   g_print("RECV: 'video-answer' -> checking\n");

                                                   if (accept_call(data->get_string().c_str()))
                                                   {
                                                       g_print("Video answer was accepted, waiting for ICE candidates...\n");
                                                   }
                                                   else
                                                   {
                                                       g_print("Video answer was rejected, now quitting...\n");
                                                       cleanup_and_quit_loop("ERROR: Failed to setup call!",
                                                                             PEER_CALL_ERROR);
                                                   }
                                               }
                                               else
                                               {
                                                   g_printerr("RECV: 'video'answer', but app is in wrong state!\n");
                                               }
                                               _lock.unlock();
                                           }));

    current_socket->on("new-ice-candidate", sio::socket::event_listener_aux(
                                                [&](string const &name, sio::message::ptr const &data,
                                                    bool isAck, sio::message::list &ack_resp)
                                                {
                                                    _lock.lock();
                                                    g_print("RECV: 'new-ice-candidate' -> adding...\n");

                                                    if (add_ice_candidate(data->get_string().c_str()))
                                                    {
                                                        g_print("ICE candidate was added\n");
                                                    }
                                                    else
                                                    {
                                                        g_print("ICE candidate was rejected, now quitting...\n");
                                                        cleanup_and_quit_loop("ERROR: Failed to setup call!",
                                                                              PEER_CALL_ERROR);
                                                    }

                                                    _lock.unlock();
                                                }));

    current_socket->on("hang-up", sio::socket::event_listener_aux(
                                      [&](string const &name, sio::message::ptr const &data,
                                          bool isAck, sio::message::list &ack_resp)
                                      {
                                          _lock.lock();
                                          g_print("RECV: 'hang-up' -> \n");
                                          _lock.unlock();
                                      }));

    current_socket->on("message", sio::socket::event_listener_aux(
                                      [&](string const &name, sio::message::ptr const &data,
                                          bool isAck, sio::message::list &ack_resp)
                                      {
                                          _lock.lock();
                                          g_print("RECV: 'message' -> \n");
                                          _lock.unlock();
                                      }));

    current_socket->on("connect_error", sio::socket::event_listener_aux(
                                            [&](string const &name, sio::message::ptr const &data,
                                                bool isAck, sio::message::list &ack_resp)
                                            {
                                                _lock.lock();
                                                g_print("RECV: 'connect_error' -> \n");
                                                _lock.unlock();
                                            }));
}

/**
 * Send a response to SignalServer's 'init' message (register us as a receiver).
 */
static void response_to_init(sio::socket::ptr current_socket)
{
    // emit: {'sub':'Player-Gstreamer', 'role': 'receiver'}
    // sub: Any name that we want to use from ourself, here 'Player-Gstreamer'.
    // role: We will be a 'receiver', the 360 camera will be a 'sender'.

    // The response must be sent as a JSON object - NOT just a string that
    // looks like JSON. Hence, we need to use Socket.IO's object_message type:
    sio::message::ptr jsonObj = sio::object_message::create();
    jsonObj->get_map()["sub"] = sio::string_message::create(own_id);
    jsonObj->get_map()["role"] = sio::string_message::create("receiver");
    g_print("SEND: 'init', {'sub':'%s', 'role': 'receiver'} \n", own_id);
    current_socket->emit(
        "init", jsonObj, [&](sio::message::list const &msg)
        {
            // The SignalServer will response with an ACK and a boolean status.
            g_print("ACK:  'init', ");
            if (msg.size() > 0)
            {
                sio::message::ptr ack = msg[0]; // There should be only one msg.
                switch (ack->get_flag())
                {
                case sio::message::flag_boolean:
                {
                    // ACK to 'init' message should contain a boolean response.
                    bool initialized = ack->get_bool();
                    if (initialized)
                    {
                        g_print("registration OK\n");
                        app_state = SERVER_REGISTERED;

                        bind_events();
                    }
                    else
                    {
                        g_print("registration FAILED\n");
                        app_state = SERVER_REGISTRATION_ERROR;

                        cleanup_and_quit_loop("Server registration failed", APP_STATE_ERROR);
                    }
                    break;
                }
                /*
                case sio::message::flag_integer:
                case sio::message::flag_double:
                case sio::message::flag_string:
                case sio::message::flag_binary:
                case sio::message::flag_array:
                case sio::message::flag_object:
                case sio::message::flag_null:
                */
                default:
                {
                    g_printerr("Unexpected response type from signaling server: %d",
                               ack->get_flag());
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
    // The original webrtc-sendrecv example uses libsoup to connect to their
    // websocket server. With a LiveSYNC enabled 360 camera, we use Socket.IO
    // for signaling, and thus replace libsoup with Socket.IO cpp client.
    // This function completely replaces the one in the original example.

    app_state = SERVER_CONNECTING;

    // First, setup connection listener for Socket.IO client.
    connection_listener l(client);
    client.set_open_listener(std::bind(&connection_listener::on_connected, &l));
    client.set_close_listener(std::bind(&connection_listener::on_close, &l,
                                        std::placeholders::_1));
    client.set_fail_listener(std::bind(&connection_listener::on_fail, &l));

    // Second, try to connect to the given server URL.
    g_print("Connecting to SignalingServer %s ...\n", server_url);
    //client.set_logs_verbose();
    client.connect(server_url);

    // ... wait until connected or connection attempt failed ...
    _lock.lock();
    if (!connect_finish)
    {
        _cond.wait(_lock);
    }
    _lock.unlock();

    // Open socket for sending/receiving messages.
    current_socket = client.socket();

    // The SignalServer uses the default namespace '/'.
    g_print("Namespace: %s\n", current_socket->get_namespace().c_str());

    // Upon connect, the SignalServer sends 'init' request and we must respond.
    // (VS Code auto-format does a bad job formatting here... sorry!)
    current_socket->on("init", sio::socket::event_listener_aux(
                                   [&](string const &name, sio::message::ptr const &data,
                                       bool isAck, sio::message::list &ack_resp)
                                   {
                                       _lock.lock();

                                       bool response = false;
                                       if (app_state == SERVER_CONNECTED)
                                       {
                                           g_print("RECV: 'init' -> Attempt to register...\n");
                                           app_state = SERVER_REGISTERING;
                                           response = true;
                                       }

                                       _cond.notify_all();
                                       _lock.unlock();

                                       // No need to listen to "init" anymore.
                                       current_socket->off("init");

                                       if (response)
                                       {
                                           response_to_init(current_socket);
                                       }
                                   }));
}

/**
 * Check that the required Gstreamer plugins are installed and available.
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
 * Main function - the program starts from here.
 */
int main(int argc, char *argv[])
{
    g_print("*** LiveSYNC Gstreamer example ***\n");
    app_state = APP_STATE_INITIALIZING;

    // Parse command-line parameters.
    GOptionContext *context;
    GError *error = nullptr;
    context = g_option_context_new("*** LiveSYNC GStreamer example ***");
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_add_group(context, gst_init_get_option_group());
    if (!g_option_context_parse(context, &argc, &argv, &error))
    {
        g_printerr("Error in initializing: %s\n", error->message);
        return -1;
    }

    // Check required command-line configuration parameters.
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

    // Check required Gstreamer plugins.
    if (!check_plugins())
        return -1;

    // Disable ssl when running on a localhost server, because
    // it's probably a test server with a self-signed certificate.
    {
        GstUri *uri = gst_uri_from_string(server_url);
        if (g_strcmp0("localhost", gst_uri_get_host(uri)) == 0 ||
            g_strcmp0("127.0.0.1", gst_uri_get_host(uri)) == 0)
            disable_ssl = TRUE;
        gst_uri_unref(uri);
    }

    // Create the main loop, which keeps the app running.
    loop = g_main_loop_new(nullptr, FALSE);

    // Setup user input for handling commands during streaming.
    GIOChannel *channel = g_io_channel_unix_new(STDIN_FILENO);
    g_io_channel_set_encoding(channel, NULL, &error);
    //prompt();
    g_io_add_watch(channel, G_IO_IN, mycallback, NULL);

    // Begin operation by attempting to connect to the signal server.
    connect_to_socketio_server_async();

    // Start the main loop and run it until we quit for some reason.
    g_main_loop_run(loop);

    // Main loop has stopped, cleanup.
    g_main_loop_unref(loop);
    g_print("Stopping Gstreamer pipeline...");
    if (pipe1)
    {
        gst_element_set_state(GST_ELEMENT(pipe1), GST_STATE_NULL);
        gst_object_unref(pipe1);
        g_printerr(" OK\n");
    }
    else
    {
        g_printerr(" Not found\n");
    }

    g_print("All done.\n");

    return 0;
}
