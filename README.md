# GStreamer 360° Video Tutorial

A simple project where we receive a live video stream from a 360° camera into a GStreamer pipeline.

## Abstract

The purpose of this project is to demonstrate how we can use a 360° camera in a GStreamer pipeline based on Finwe's LiveSYNC solution and standard WebRTC technologies.

As a starting point, we will use a 360° camera with Android operating system and Finwe's LiveSYNC Camera app installed to it, as well as Finwe's dockerized SignalingServer for messaging (WebRTC requires signaling but does not include it) in a local home/office network.

We will build a simple Gstreamer pipeline on an Ubuntu Linux host, make it communicate with the 360° camera via the SignalingServer using websockets, and then receive a live 360° videostream using peer-to-peer WebRTC connection. The video stream can be further manipulated with other GStreamer plugins in the pipeline to implement a desired use case.

## Hardware

The following hardware is required:
1. Labpano Pilot or Labpano Era 360° camera
2. A PC for running Docker and Finwe's SignalingServer (desktop, laptop, or a headless box such as NVidia Xavier devkit)
3. A PC (or a VM) for running Ubuntu Linux and a Gstreamer pipeline that we will build (can be the same PC where the SignalingServer is running, but doesn't have to)
4. Local Wifi/Ethernet network for communication

## Initial setup

1. Install Finwe's LiveSYNC Camera app to Labpano Pilot or Era camera, if not already installed (*Settings->Applications->Install App*)
2. Install Finwe's SignalingServer (docker image) to a PC, if not already installed (install Docker, copy and load the image)
3. Configure LiveSYNC camera app via its settings.ini file in the camera (edit /Android/data/fi.finwe.livesync.camera.labpano.pilot/files/settings.ini and ensure 'webrtc_signalling_server_url' points to the PC running the SignalingServer)
4. Run the SignalingServer (e.g. 'docker run -p 443:443 finwe/signaling-server:0.0.22-amd64'), if not configured to auto-start. You should see this in the console when the SignalingServer is ready:
```
docker run -p 443:443 finwe/signaling-server:0.0.22-amd64
[10.08.06] Server listening at port 443
```
5. Start LiveSYNC Camera app in the camera. It should connect to the SignalingServer, show "Live View / Waiting for viewer to connect...", and a video preview on screen. In the SignalingServer's console/log you should see something like this:
```
[10.18.06] Socket connected
[10.18.06] SEND: 'init': Server --> Connected socket
[10.18.06] RECV: 'init'
[10.18.06] 	New connection: ID: 8v7UmFbxrMZM2K6pAAAA Sub: LiveSYNC Camera Role: sender
[10.18.06] Receivers not found
[10.18.06] Receivers not found
[10.18.06] RECV: 'client-count'
[10.18.06] Receivers not found
[10.18.06] RECV: 'ready': LiveSYNC Camera
[10.18.06] Receivers not found
```

The setup is now ready and working, waiting for a video client to show up and start viewing a video stream.

## Testing the setup with the SignalingServer's bundled web player

You can test the setup with a web player that Finwe has developed and bundled with the SignalingServer. Simply open a browser window to 

```
https://192.168.100/
``` 

, or whatever is the IP address of your SignalingServer machine. Notice that **using HTTPS is mandatory** and you need to **create a security exception in Chrome to proceed to the player's web page** (or add the SignalingServer's certificate to your browser). WebRTC requires that all communication is encrypted.

The player will then appear in the browser, the camera will be found, and you can start streaming by clicking the play icon. Live 360° video stream appears on screen. You can, for example, drag the view with mouse to look at different directions.

### Web player's debug messages

If you hit F12 to open Chrome's debug tools, you should find messages like this in the console, showing the communication from the web player's point of view:

```
Overlay mounted
index.js:33 Signaling state: CONNECTED
index.js:33 Signaling state: AUTHENTICATING
index.js:37 Device state: AVAILABLE
index.js:37 Device state: READY
signaling.js:192 client-count: {"max-streaming-clients":1,"streaming-clients":0,"max-connected-clients":1,"connected-clients":0}
index.js:37 Device state: READY
signaling.js:176 video-format: {"projection":"equirectangular"}
index.js:33 Signaling state: AUTHENTICATED
index.js:29 App state: IDLE
index.js:50 Splash: false
index.js:29 App state: INITIALIZING
Player.vue:755 [13:26:26] Inviting user LiveSYNC Camera
Player.vue:755 [13:26:26] Setting up connection to invite user: LiveSYNC Camera
Player.vue:755 [13:26:26] Setting up a connection...
Player.vue:730 RTCPeerConnection config: null
Player.vue:755 [13:26:26] addTransceivers
Player.vue:755 [13:26:26] *** Negotiation needed
Player.vue:755 [13:26:26] ---> Creating offer
Player.vue:755 [13:26:26] ---> Setting local description to the offer
Player.vue:755 [13:26:26] *** WebRTC signaling state changed to: have-local-offer
Player.vue:755 [13:26:26] ---> Sending the offer to the remote peer
Player.vue:755 [13:26:26] *** ICE gathering state changed to: gathering
Player.vue:755 [13:26:26] *** Outgoing ICE candidate: candidate:3105591770 1 udp 2113937151 349e5fb9-727a-47de-b5ac-45b5d4f442ca.local 62253 typ host generation 0 ufrag U9ZG network-cost 999
Player.vue:755 [13:26:26] *** Outgoing ICE candidate: candidate:3105591770 1 udp 2113937151 349e5fb9-727a-47de-b5ac-45b5d4f442ca.local 56334 typ host generation 0 ufrag U9ZG network-cost 999
Player.vue:755 [13:26:26] *** ICE gathering state changed to: complete
signaling.js:103 video-answer: {"target":"Player-0.2593126322505004","type":"video-answer","sdp":{"type":"answer","sdp":"v=0\r\no=- 8522926671151979807 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=group:BUNDLE 0 1\r\na=extmap-allow-mixed\r\na=msid-semantic: WMS 103\r\nm=video 9 UDP\/TLS\/RTP\/SAVPF 96 97 98 99 125 107 114 115 116\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:zw5A\r\na=ice-pwd:SBcg5\/+\/fTxRKGcKGONmUHmp\r\na=ice-options:trickle renomination\r\na=fingerprint:sha-256 20:5C:98:F2:FA:E4:77:E2:13:B4:96:A9:A4:F0:71:8A:B4:E0:AD:7D:63:D4:1F:75:C8:9C:A6:0D:18:88:86:5C\r\na=setup:active\r\na=mid:0\r\na=extmap:1 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:2 http:\/\/www.webrtc.org\/experiments\/rtp-hdrext\/abs-send-time\r\na=extmap:3 urn:3gpp:video-orientation\r\na=extmap:4 http:\/\/www.ietf.org\/id\/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\na=extmap:5 http:\/\/www.webrtc.org\/experiments\/rtp-hdrext\/playout-delay\r\na=extmap:6 http:\/\/www.webrtc.org\/experiments\/rtp-hdrext\/video-content-type\r\na=extmap:7 http:\/\/www.webrtc.org\/experiments\/rtp-hdrext\/video-timing\r\na=extmap:8 http:\/\/www.webrtc.org\/experiments\/rtp-hdrext\/color-space\r\na=sendonly\r\na=rtcp-mux\r\na=rtcp-rsize\r\na=rtpmap:96 VP8\/90000\r\na=rtcp-fb:96 goog-remb\r\na=rtcp-fb:96 transport-cc\r\na=rtcp-fb:96 ccm fir\r\na=rtcp-fb:96 nack\r\na=rtcp-fb:96 nack pli\r\na=rtpmap:97 rtx\/90000\r\na=fmtp:97 apt=96\r\na=rtpmap:98 VP9\/90000\r\na=rtcp-fb:98 goog-remb\r\na=rtcp-fb:98 transport-cc\r\na=rtcp-fb:98 ccm fir\r\na=rtcp-fb:98 nack\r\na=rtcp-fb:98 nack pli\r\na=rtpmap:99 rtx\/90000\r\na=fmtp:99 apt=98\r\na=rtpmap:125 H264\/90000\r\na=rtcp-fb:125 goog-remb\r\na=rtcp-fb:125 transport-cc\r\na=rtcp-fb:125 ccm fir\r\na=rtcp-fb:125 nack\r\na=rtcp-fb:125 nack pli\r\na=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\na=rtpmap:107 rtx\/90000\r\na=fmtp:107 apt=125\r\na=rtpmap:114 red\/90000\r\na=rtpmap:115 rtx\/90000\r\na=fmtp:115 apt=114\r\na=rtpmap:116 ulpfec\/90000\r\na=ssrc-group:FID 1052756425 307003892\r\na=ssrc:1052756425 cname:etcl\/8rXTyXz3WmW\r\na=ssrc:1052756425 msid:103 101\r\na=ssrc:1052756425 mslabel:103\r\na=ssrc:1052756425 label:101\r\na=ssrc:307003892 cname:etcl\/8rXTyXz3WmW\r\na=ssrc:307003892 msid:103 101\r\na=ssrc:307003892 mslabel:103\r\na=ssrc:307003892 label:101\r\nm=audio 9 UDP\/TLS\/RTP\/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:zw5A\r\na=ice-pwd:SBcg5\/+\/fTxRKGcKGONmUHmp\r\na=ice-options:trickle renomination\r\na=fingerprint:sha-256 20:5C:98:F2:FA:E4:77:E2:13:B4:96:A9:A4:F0:71:8A:B4:E0:AD:7D:63:D4:1F:75:C8:9C:A6:0D:18:88:86:5C\r\na=setup:active\r\na=mid:1\r\na=extmap:14 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\na=extmap:2 http:\/\/www.webrtc.org\/experiments\/rtp-hdrext\/abs-send-time\r\na=extmap:4 http:\/\/www.ietf.org\/id\/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\na=sendonly\r\na=rtcp-mux\r\na=rtpmap:111 opus\/48000\/2\r\na=rtcp-fb:111 transport-cc\r\na=fmtp:111 minptime=10;useinbandfec=1\r\na=rtpmap:103 ISAC\/16000\r\na=rtpmap:104 ISAC\/32000\r\na=rtpmap:9 G722\/8000\r\na=rtpmap:0 PCMU\/8000\r\na=rtpmap:8 PCMA\/8000\r\na=rtpmap:106 CN\/32000\r\na=rtpmap:105 CN\/16000\r\na=rtpmap:13 CN\/8000\r\na=rtpmap:110 telephone-event\/48000\r\na=rtpmap:112 telephone-event\/32000\r\na=rtpmap:113 telephone-event\/16000\r\na=rtpmap:126 telephone-event\/8000\r\na=ssrc:3070692291 cname:etcl\/8rXTyXz3WmW\r\na=ssrc:3070692291 msid:103 102\r\na=ssrc:3070692291 mslabel:103\r\na=ssrc:3070692291 label:102\r\n"}}
Player.vue:755 [13:26:27] handleVideoAnswerMsg
Player.vue:755 [13:26:27] *** Call recipient has accepted our call
index.js:29 App state: INITIALIZING
index.js:37 Device state: undefined
signaling.js:114 new-ice-candidate: {"target":"Player-0.2593126322505004","type":"new-ice-candidate","candidate":{"candidate":"candidate:3870334310 1 udp 2122260223 192.168.1.133 50356 typ host generation 0 ufrag zw5A network-id 3 network-cost 10","sdpMid":"0","sdpMLineIndex":0}}
Player.vue:755 [13:26:27] handleNewICECandidateMsg
Player.vue:755 [13:26:27] [object Object]
Player.vue:755 [13:26:27] *** Adding received ICE candidate: {"candidate":"candidate:3870334310 1 udp 2122260223 192.168.1.133 50356 typ host generation 0 ufrag zw5A network-id 3 network-cost 10","sdpMid":"0","sdpMLineIndex":0}
Player.vue:755 [13:26:27] *** WebRTC signaling state changed to: stable
2Player.vue:755 [13:26:27] *** Track event
signaling.js:114 new-ice-candidate: {"target":"Player-0.2593126322505004","type":"new-ice-candidate","candidate":{"candidate":"candidate:559267639 1 udp 2122202367 ::1 53057 typ host generation 0 ufrag zw5A network-id 2","sdpMid":"0","sdpMLineIndex":0}}
Player.vue:755 [13:26:27] handleNewICECandidateMsg
Player.vue:755 [13:26:27] [object Object]
Player.vue:755 [13:26:27] *** Adding received ICE candidate: {"candidate":"candidate:559267639 1 udp 2122202367 ::1 53057 typ host generation 0 ufrag zw5A network-id 2","sdpMid":"0","sdpMLineIndex":0}
signaling.js:114 new-ice-candidate: {"target":"Player-0.2593126322505004","type":"new-ice-candidate","candidate":{"candidate":"candidate:1510613869 1 udp 2122129151 127.0.0.1 52671 typ host generation 0 ufrag zw5A network-id 1","sdpMid":"0","sdpMLineIndex":0}}
Player.vue:755 [13:26:27] handleNewICECandidateMsg
Player.vue:755 [13:26:27] [object Object]
Player.vue:755 [13:26:27] *** Adding received ICE candidate: {"candidate":"candidate:1510613869 1 udp 2122129151 127.0.0.1 52671 typ host generation 0 ufrag zw5A network-id 1","sdpMid":"0","sdpMLineIndex":0}
signaling.js:114 new-ice-candidate: {"target":"Player-0.2593126322505004","type":"new-ice-candidate","candidate":{"candidate":"candidate:1876313031 1 tcp 1518222591 ::1 44397 typ host tcptype passive generation 0 ufrag zw5A network-id 2","sdpMid":"0","sdpMLineIndex":0}}
Player.vue:755 [13:26:27] handleNewICECandidateMsg
Player.vue:755 [13:26:27] [object Object]
Player.vue:755 [13:26:27] *** Adding received ICE candidate: {"candidate":"candidate:1876313031 1 tcp 1518222591 ::1 44397 typ host tcptype passive generation 0 ufrag zw5A network-id 2","sdpMid":"0","sdpMLineIndex":0}
signaling.js:114 new-ice-candidate: {"target":"Player-0.2593126322505004","type":"new-ice-candidate","candidate":{"candidate":"candidate:344579997 1 tcp 1518149375 127.0.0.1 53983 typ host tcptype passive generation 0 ufrag zw5A network-id 1","sdpMid":"0","sdpMLineIndex":0}}
Player.vue:755 [13:26:27] handleNewICECandidateMsg
Player.vue:755 [13:26:27] [object Object]
Player.vue:755 [13:26:27] *** Adding received ICE candidate: {"candidate":"candidate:344579997 1 tcp 1518149375 127.0.0.1 53983 typ host tcptype passive generation 0 ufrag zw5A network-id 1","sdpMid":"0","sdpMLineIndex":0}
Player.vue:755 [13:26:27] *** ICE connection state changed to checking
Player.vue:755 [13:26:27] *** ICE connection state changed to connected
index.js:29 App state: STREAMING
signaling.js:192 client-count: {"connected-clients":1,"max-connected-clients":1,"streaming-clients":1,"max-streaming-clients":1}
index.js:37 Device state: BUSY
signaling.js:176 video-format: {"projection":"equirectangular"}
index.js:50 Splash: false
```

### SignalingServer's debug log

In addition, something like this will appears in the SignalingServer's log, showing how messages are transmitted between the 360° camera and the web player.

```
[10.25.20] Socket connected
[10.25.20] SEND: 'init': Server --> Connected socket
[10.25.20] RECV: 'init'
[10.25.20] 	New connection: ID: tyF9kpcGZNdEkFidAAAB Sub: Player-0.2593126322505004 Role: receiver
[10.25.20] SEND: 'owner-authenticated': Server --> 8v7UmFbxrMZM2K6pAAAA
[10.26.26] RECV: 'video-offer': Player-0.2593126322505004 --> LiveSYNC Camera
[10.26.26] SEND: 'video-offer': Server --> LiveSYNC Camera
[10.26.26] RECV: 'new-ice-candidate': Player-0.2593126322505004 --> LiveSYNC Camera
[10.26.26] SEND: 'new-ice-candidate': Server --> LiveSYNC Camera
[10.26.26] RECV: 'new-ice-candidate': Player-0.2593126322505004 --> LiveSYNC Camera
[10.26.26] SEND: 'new-ice-candidate': Server --> LiveSYNC Camera
[10.26.27] RECV: 'video-answer': LiveSYNC Camera --> Player-0.2593126322505004
[10.26.27] SEND: 'video-answer': Server --> Player-0.2593126322505004
[10.26.27] RECV: 'new-ice-candidate': LiveSYNC Camera --> Player-0.2593126322505004
[10.26.27] SEND: 'new-ice-candidate': Server --> Player-0.2593126322505004
[10.26.27] RECV: 'new-ice-candidate': LiveSYNC Camera --> Player-0.2593126322505004
[10.26.27] SEND: 'new-ice-candidate': Server --> Player-0.2593126322505004
[10.26.27] RECV: 'new-ice-candidate': LiveSYNC Camera --> Player-0.2593126322505004
[10.26.27] SEND: 'new-ice-candidate': Server --> Player-0.2593126322505004
[10.26.27] RECV: 'new-ice-candidate': LiveSYNC Camera --> Player-0.2593126322505004
[10.26.27] SEND: 'new-ice-candidate': Server --> Player-0.2593126322505004
[10.26.27] RECV: 'new-ice-candidate': LiveSYNC Camera --> Player-0.2593126322505004
[10.26.27] SEND: 'new-ice-candidate': Server --> Player-0.2593126322505004
[10.26.27] 	ACK: 'video-answer' Server --> Player-0.2593126322505004
[10.26.27] 	ACK: 'new-ice-candidate' Server --> Player-0.2593126322505004
[10.26.27] 	ACK: 'new-ice-candidate' Server --> Player-0.2593126322505004
[10.26.27] 	ACK: 'new-ice-candidate' Server --> Player-0.2593126322505004
[10.26.27] 	ACK: 'new-ice-candidate' Server --> Player-0.2593126322505004
[10.26.27] 	ACK: 'new-ice-candidate' Server --> Player-0.2593126322505004
[10.26.27] RECV: 'client-count'
[10.26.27] SEND: 'client-count': Server --> tyF9kpcGZNdEkFidAAAB
[10.26.27] RECV: 'video-format'
[10.26.27] SEND: 'video-format': Server --> tyF9kpcGZNdEkFidAAAB
[10.26.27] 	ACK: 'client-count' Server --> tyF9kpcGZNdEkFidAAAB
[10.26.27] 	ACK: 'video-format' Server --> tyF9kpcGZNdEkFidAAAB
```

### LiveSYNC Camera app's debug log

If you have a debug build of the LiveSYNC Camera app, you can connect the camera via USB to a PC that has ADB (Android Debug Bridge), and use for example Android Studio to view the camera's debug log.

> Notice that it isn't possible to view a debug log from the LiveSYNC Camera app when a release build of the app is being used.

### Chrome's WebRTC debug tools

Chrome web browser has pleanty of debug information available specifically for debugging WebRTC connections. When you have an ongoing WebRTC stream in one tab, simply open another tab and navigate to

```
chrome://webrtc-internals
```

There are pleanty of stats and also dozens of live-updating graphs. Extremely useful when you already have established a connection and need to debug for example video quality issues.

### Testing control commands

The SignalingServer is required for establishing a connection between the peers, ie. the 360° camera and the web player. Video playback does not require it anymore after streaming begins. 

However, we have implemented several control commands for exchanging information between the player and the camera. These are transmitted as control commands from the web player to the camera via the SignalingServer. In addition, the camera send messages to the web player to inform e.g. how many simultaneous clients it supports and whether there is room to join, or not.

For example, you can click a '+' or '-' button in the web player's UI to zoom in/out. As a result of a button click, the web player will send a message to the camera. You can see this message in the SignalingServer's debug log:

```
[10.38.20] RECV: 'message': zoom-in
[10.38.20] SEND: 'message': Server --> LiveSYNC Camera
```

## Ubuntu Linux host

The steps above were just a preparation for the actual task: building a GStreamer pipeline with WebRTC input from the 360° camera. We will now proceed to replacing the web player with GStreamer as a video client.

The first step is to setup a host machine. Here we will install Ubuntu 18.04 into VirtualBox on a Mac, but you might as well use a different host machine (such as Windows), or install Ubuntu natively to a PC or headless box (such as NVidia Xavier devkit).

### Installing Ubuntu on VirtualBox

It is not in the scope of this project to guide this step-by-step. Follow for example this blog post:
https://codingwithmanny.medium.com/installing-ubuntu-18-04-on-mac-os-with-virtualbox-ac3b39678602

Install the usual updates to the OS/apps after the installer has finished and Ubuntu booted for the first time.

## Installing GStreamer

Run this in terminal to install GStreamer:
````
> sudo apt-get install gstreamer1.0-tools gstreamer1.0-alsa gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav
````

To compile programs for GStreamer pipeline, we also need the dev packages:
`````
sudo apt-get install libgstreamer1.0-dev
````

Create a directory for your source files and go there:
````
> mkdir ~/Source
> cd ~/Source
`````

Create 'hello world':
````
> nano basic-tutorial-1.c
`````
Copy-paste this:
````
#include <gst/gst.h>

int
main (int argc, char *argv[])
{
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Build the pipeline */
  pipeline =
      gst_parse_launch
      ("playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm",
      NULL);

  /* Start playing */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
  msg =
      gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  /* Free resources */
  if (msg != NULL)
    gst_message_unref (msg);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}
````
Save and exit by *CTRL-O*, then *CTRL-X*.

Install required packages for compiling the program:
````
sudo apt-get install pkg-config
````

Then, compile the program:
````
gcc basic-tutorial-1.c -o basic-tutorial-1 `pkg-config --cflags --libs gstreamer-1.0`
```

Run the program:
````
./basic-tutorial-1
````

This will play a video stream from network in a window on the Ubuntu desktop.

GStreamer and development environment is now successfully installed.

