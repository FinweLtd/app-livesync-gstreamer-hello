# GStreamer 360° Video Tutorial

A simple project where we receive a live video stream from a 360° camera into a GStreamer pipeline.

## Abstract

The purpose of this project is to demonstrate how we can use a 360° camera in a GStreamer pipeline based on Finwe's LiveSYNC solution and standard WebRTC technologies.

As a starting point, we will use a 360° camera with Android operating system and Finwe's LiveSYNC Camera app installed to it, as well as Finwe's dockerized SignalingServer for messaging (WebRTC requires signaling but does not include it) in a local home/office network.

We will build a simple Gstreamer pipeline on an Ubuntu Linux VM, make it communicate with the 360° camera via the SignalingServer using websockets, and then receive a live 360° videostream using peer-to-peer WebRTC connection. The video stream can be further manipulated with other GStreamer plugins in the pipeline to implement a desired use case.

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
> docker run -p 443:443 finwe/signaling-server:0.0.22-amd64
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
https://192.168.1.100/
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

However, we have implemented several control commands for exchanging information between the player and the camera. These are transmitted from the web player to the camera via the SignalingServer. In addition, the camera send messages to the web player to inform e.g. how many simultaneous clients it supports and whether there is room to join, or not.

For example, you can click a '+' or '-' button in the web player's UI to zoom in/out. As a result of the button click, the web player will send a message to the camera. You can see this message in the SignalingServer's console/log:

```
[10.38.20] RECV: 'message': zoom-in
[10.38.20] SEND: 'message': Server --> LiveSYNC Camera
```

## Ubuntu Linux host

The steps above were just a preparation for the actual task: building a GStreamer pipeline with WebRTC input from the 360° camera. We will now proceed to replace the web player with GStreamer as the video client.

The first step is to setup Ubuntu. Here we will install Ubuntu 18.04 into VirtualBox on a Mac, but you might as well use a different host machine such as Windows for running the VM, or install Ubuntu natively to a PC or a headless box such as NVidia Xavier devkit.

### Installing Ubuntu on VirtualBox

It is not in the scope of this project to guide this step-by-step. Follow for example this blog post:

https://codingwithmanny.medium.com/installing-ubuntu-18-04-on-mac-os-with-virtualbox-ac3b39678602

Install the usual updates to the OS/apps after the installer has finished and Ubuntu booted for the first time. Enabling copy-pasting and file sharing with the host machine is recommended.

To get a webcam working for WebRTC examples/demos inside Ubuntu VM, download and install VirtualBox extensions from here and then reboot both the host and guest OS, and select Devices -> Webcames -> [your webcam].

https://www.virtualbox.org/wiki/Downloads

For example Cheese did not seem to work like this, but capturing a snapshot with fswebcam did work:

```
> fswebcam -r 1920x1080 --jpeg 90 --save test.jpg
```

Also, live video stream worked through guvcview:
```
> guvcview
```

Live video worked also via VLC (although, with poor frame rate):
```
> vlc v4l2:///dev/video0:chroma=mjpg:width=1920:height=1080
```

The camera was also listed via v4l2:
```
> v4l2-ctl --list-devices
```

Once completed, start Ubuntu and log in to the desktop environment. Perform the next steps in this environment.

## Installing GStreamer

Run this in terminal to install GStreamer:
````
> sudo apt-get install gstreamer1.0-tools gstreamer1.0-alsa gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav
````

In order to compile programs for GStreamer, we also need the development packages:
````
> sudo apt-get install libgstreamer1.0-dev
````

Create a directory for your source files, and navigate there:
````
> mkdir ~/Source
> cd ~/Source
`````

Create a new source code file for a simple 'hello world' application:
````
> nano basic-tutorial-1.c
`````
Copy-paste this code to the text editor:
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
Save the file with *CTRL-O* and then exit with *CTRL-X*.

Install required packages for compiling the program:
````
> sudo apt-get install pkg-config
````

Compile the program:
```
> gcc basic-tutorial-1.c -o basic-tutorial-1 `pkg-config --cflags --libs gstreamer-1.0`
```

Run the program:
````
./basic-tutorial-1
````

This will play a video stream from network in a window on the Ubuntu desktop.

> GStreamer and development environment is now successfully installed.

## About GStreamer version

GStreamer comes with a large collection of plugins and one of them is for adding support for WebRTC. This is a fairly new plugin, and just like WebRTC itself, has been under heavy development during the past few years. The version that can be found from Ubuntu's package repository is related to GStreamer version, which is a fairly old one.

Run this to check the currently installed GStreamer version:
```
> gst-launch-1.0 --gst-version
GStreamer Core Library version 1.14.5
```

Unfortunately, installing a newer version is not a simple task. GStreamer download page contains pre-built binaries for Windows, Mac, Android and iOS. Linux users, as usual, need to use the version from the package manager, or build another version from source code.

So, you could upgrade Ubuntu itself to a newer version, such as 20.04, to get a newer version of GStreamer. Upgrading only GStreamer libraries typically means compiling them all from sources, and there are lots of dependencies to handle.

Here we start with the version available for Ubuntu 18.04, which is 1.14.5.

## GStreamer and webcam

For testing WebRTC examples, we should have a webcam available in Ubuntu. Nowadays, webcams work pretty well in Linux, but it is still possible to encounter devices that just won't work. When Ubuntu is running inside VirtualBox or other virtualization technology, it can be much more trickier to get a webcam working.

With GStreamer, you can try to following after ensuring that a webcam works at least with fswebcam and guvcview:
```
> v4l2-ctl --list-devices
> v4l2-ctl --list-formats-ext
> gst-launch-1.0 v4l2src device="/dev/video0" ! "image/jpeg, width=640, height=480, framerate=5/1, format=MJPG" ! jpegdec ! autovideosink
```

This worked on a Macbook Pro running Ubuntu 18.04 on VirtualBox and using Logitech HD Pro Webcam c920 USB webcam.

## GStreamer WebRTC demos

Now that GStreamer is tested to work, let's proceed to testing the WebRTC plugin. There are a few demos that are documented here:

https://github.com/centricular/gstwebrtc-demos

The repository has been moved and its instructions are now outdated, but we will use it here anyway as our WebRTC plugin is an old version, too.

### Cloning the examples repository

Create a directory for the examples, and navigate there.
````
> cd ~/Source
> mkdir examples
> cd examples
`````

Install git:
````
> sudo apt-get install git
````

Clone the examples repository using git:
````
> git clone https://github.com/centricular/gstwebrtc-demos.git
````

### Running a basic WebRTC example

#### Web GUI

Install Python2:
````
> sudo apt install python
`````

Serve the web GUI:
````
> cd ~/Source/examples/gstwebrtc-demos/sendrecv/js
> python -m SimpleHTTPServer 8080
````

Open a web browser and navigate to:
````
http://localhost:8080
````

This will open a simple GUI, but it fill fail after few attempts to connect to the signaling server that comes with the examples - we haven't started that yet.

#### Signaling server

For running the signaling server, we need to add websockets to the current Python3 installation. First, let's install pip so we can easily add Python modules:

````
> sudo apt install python3-pip
````

Now, install websocket module:
````
> pip3 install websocket websockets
````

Before starting the signaling server, we need to generate a certificate for HTTPS. The examples have a script for that:
````
> ./generate_cert.sh
````

Now we can start the signaling server for the examples:
````
python3 simple_server.py
`````

#### Security exception

Return to browser, and navigate to:
````
https://localhost:8443/health
`````

Remember to use HTTPS here. Firefox warns about a security risk, as the certificate was self-generated. Click Advanced and accept the exception.

The page should load and show a simple 'OK' message at the top.

#### Web GUI, returned

Now, still using the browser, return to:
````
http://localhost:8080
````

This time, the GUI should be able to connect to the signaling server and show this status: "Registered with server, waiting for call". There is also a text showing peer ID, for example "5092".

Also, the signaling server's console output should show something like this:

````
Starting server...
Using TLS with keys in ''
Listening on https://:8443
Connected to ('127.0.0.1', 39822)
Registered peer '5092' at ('127.0.0.1', 39822)
Sending keepalive ping to ('127.0.0.1', 39822) in recv
Sending keepalive ping to ('127.0.0.1', 39822) in recv
Sending keepalive ping to ('127.0.0.1', 39822) in recv
Sending keepalive ping to ('127.0.0.1', 39822) in recv
````

Notice how the same peer ID 5092 appears in the log as a registered peer. 

The signaling server and web GUI are now OK, next we need to compile a WebRTC client.

#### Send-Receive WebRTC Client (Python)

In terminal, navigate to:
````
> cd ~/Source/examples/gstwebrtc-demos/sendrecv/gst
````

We should be able to run the Python version of the example, but if fails as follows:
````
> python3 webrtc_sendrecv.py 
Traceback (most recent call last):
  File "webrtc_sendrecv.py", line 13, in <module>
    gi.require_version('GstWebRTC', '1.0')
  File "/usr/lib/python3/dist-packages/gi/__init__.py", line 130, in require_version
    raise ValueError('Namespace %s not available' % namespace)
ValueError: Namespace GstWebRTC not available
````

To fix this issue, install this package:
````
> sudo apt install gir1.2-gst-plugins-bad-1.0
````

Next, we get another issues:
````
> python3 webrtc_sendrecv.py 
Missing gstreamer plugins: ['nice']
````

This happens, because the current GStreamer installation does not contain the required 'nice' plugin:
````
gst-inspect-1.0 nice
[Nothing found]
````

To fix this issue, install this package:
````
> sudo apt-get install gstreamer1.0-nice
````

Now the plugin can be found:
````
> gst-inspect-1.0 nice
Plugin Details:
  Name                     nice
  Description              Interactive UDP connectivity establishment
  Filename                 /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstnice.so
  Version                  0.1.14
  License                  LGPL
  Source module            libnice
  Binary package           libnice
  Origin URL               http://telepathy.freedesktop.org/wiki/

  nicesrc: ICE source
  nicesink: ICE sink

  2 features:
  +-- 2 elements
`````

If you try to run it again, all the components can be found, but we get an error about missing peer ID:
````
> python3 webrtc_sendrecv.py 
usage: webrtc_sendrecv.py [-h] [--server SERVER] peerid
webrtc_sendrecv.py: error: the following arguments are required: peerid
````

In addition to peer ID, we actually need to specify the server as well, else the script will use a publicly available signaling server. To keep things simple, let's start with this default (public) signaling server. Open a browser and navigate here:

````
https://webrtc.nirbheek.in/
````

Notice the peer ID, e.g. 2767, and try to run the script again:
````
> python3 webrtc_sendrecv.py 2767
Traceback (most recent call last):
  File "webrtc_sendrecv.py", line 57, in on_offer_created
    offer = reply['offer']
TypeError: 'Structure' object is not subscriptable
````

This should have worked, but there is a bug in the Python script. We'll fix that next:
````
> nano webrtc_sendrecv.py
````
Find this text:
```
offer = reply['offer']
```
... and replace it with this:
````
offer = reply.get_value('offer')
````

Finally, the example runs:
````
> python3 webrtc_sendrecv.py 2151
Sending offer:
v=0
o=- 3027051069768005227 0 IN IP4 0.0.0.0
s=-
t=0 0
a=ice-options:trickle
a=msid-semantic:WMS sendrecv
m=video 9 UDP/TLS/RTP/SAVPF 97
c=IN IP4 0.0.0.0
a=setup:actpass
a=ice-ufrag:cBmyufUM+QHlEKu0Q4Ioo+pG2ND7/zRY
a=ice-pwd:3KyE29m6Vx11BUyxEQcTdfYgzvo7UzHq
a=sendrecv
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:97 VP8/90000
a=rtcp-fb:97 nack pli
a=framerate:30
a=ssrc:2612173537 msid:user1672718228@host-e685f0ce webrtctransceiver0
a=ssrc:2612173537 cname:user1672718228@host-e685f0ce
a=mid:video0
a=fingerprint:sha-256 43:F5:CD:A0:68:85:4E:E8:E6:41:FD:3C:48:D1:BB:5E:6B:54:B4:2D:53:26:50:E6:14:3A:C4:30:F9:51:56:E8
m=audio 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=setup:actpass
a=ice-ufrag:eqBM0z3ZCKZztmg+MK/2TED8z+dIMm3Y
a=ice-pwd:xLm7NcMASvJLNp5NYSWAQRfSQJkTBNdo
a=sendrecv
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 OPUS/48000/2
a=rtcp-fb:96 nack pli
a=fmtp:96 sprop-maxcapturerate=48000;sprop-stereo=0
a=ssrc:3308120387 msid:user1672718228@host-e685f0ce webrtctransceiver1
a=ssrc:3308120387 cname:user1672718228@host-e685f0ce
a=mid:audio1
a=fingerprint:sha-256 43:F5:CD:A0:68:85:4E:E8:E6:41:FD:3C:48:D1:BB:5E:6B:54:B4:2D:53:26:50:E6:14:3A:C4:30:F9:51:56:E8

Received answer:
v=0
o=- 1934395435330990047 2 IN IP4 127.0.0.1
s=-
t=0 0
a=msid-semantic: WMS lSiHqPoixBnlJzbsTDWoCfKr5w0g0u17IwBW
m=video 9 UDP/TLS/RTP/SAVPF 97
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:orMw
a=ice-pwd:TdZBVyBRVafpbs9tH9NksClz
a=ice-options:trickle
a=fingerprint:sha-256 D3:82:C5:60:2E:EE:F0:8C:A0:1F:79:D8:96:0D:54:2F:EA:51:98:BD:E7:15:88:E0:A9:61:66:D0:CD:36:9F:12
a=setup:active
a=mid:video0
a=sendrecv
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:97 VP8/90000
a=rtcp-fb:97 nack pli
a=ssrc:761656839 cname:cN+WwG+YITxNsA2B
a=ssrc:761656839 msid:lSiHqPoixBnlJzbsTDWoCfKr5w0g0u17IwBW 3383712c-6658-4168-bbf9-2667c78cc669
a=ssrc:761656839 mslabel:lSiHqPoixBnlJzbsTDWoCfKr5w0g0u17IwBW
a=ssrc:761656839 label:3383712c-6658-4168-bbf9-2667c78cc669
m=audio 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:0Lb0
a=ice-pwd:ifSM/7G9qEfcA81ujpNW0P5j
a=ice-options:trickle
a=fingerprint:sha-256 D3:82:C5:60:2E:EE:F0:8C:A0:1F:79:D8:96:0D:54:2F:EA:51:98:BD:E7:15:88:E0:A9:61:66:D0:CD:36:9F:12
a=setup:active
a=mid:audio1
a=sendrecv
a=rtcp-mux
a=rtpmap:96 OPUS/48000/2
a=fmtp:96 minptime=10;useinbandfec=1
a=ssrc:721885212 cname:cN+WwG+YITxNsA2B
a=ssrc:721885212 msid:lSiHqPoixBnlJzbsTDWoCfKr5w0g0u17IwBW 95ab221d-5ab6-47db-a49e-90f1ce354eb9
a=ssrc:721885212 mslabel:lSiHqPoixBnlJzbsTDWoCfKr5w0g0u17IwBW
a=ssrc:721885212 label:95ab221d-5ab6-47db-a49e-90f1ce354eb9
````

The web browser now asks permission to access camera and microphone, and the connection is established.

Next, let's try the same with the signaling server running on our own computer:
````
> python3 webrtc_sendrecv.py --server wss://localhost:8443 6797
````

The connection is established, and if you go to the web browser, you should see a bouncing ball demo running.

> The demo is not that fancy, and seems to crash every now and then... perhaps a bit of a disappoinment after all the steps :)

#### Send-Receive WebRTC Client (C)

In terminal, navigate to:
````
> cd ~/Source/examples/gstwebrtc-demos/sendrecv/gst
````

We need to compile the C code for this example, but trying to run 'make' in this directory produces errors:
````
> make
Package gstreamer-sdp-1.0 was not found in the pkg-config search path.
Perhaps you should add the directory containing `gstreamer-sdp-1.0.pc'
to the PKG_CONFIG_PATH environment variable
No package 'gstreamer-sdp-1.0' found
Package gstreamer-webrtc-1.0 was not found in the pkg-config search path.
Perhaps you should add the directory containing `gstreamer-webrtc-1.0.pc'
to the PKG_CONFIG_PATH environment variable
No package 'gstreamer-webrtc-1.0' found
Package json-glib-1.0 was not found in the pkg-config search path.
Perhaps you should add the directory containing `json-glib-1.0.pc'
to the PKG_CONFIG_PATH environment variable
No package 'json-glib-1.0' found
Package gstreamer-sdp-1.0 was not found in the pkg-config search path.
Perhaps you should add the directory containing `gstreamer-sdp-1.0.pc'
to the PKG_CONFIG_PATH environment variable
No package 'gstreamer-sdp-1.0' found
Package gstreamer-webrtc-1.0 was not found in the pkg-config search path.
Perhaps you should add the directory containing `gstreamer-webrtc-1.0.pc'
to the PKG_CONFIG_PATH environment variable
No package 'gstreamer-webrtc-1.0' found
Package json-glib-1.0 was not found in the pkg-config search path.
Perhaps you should add the directory containing `json-glib-1.0.pc'
to the PKG_CONFIG_PATH environment variable
No package 'json-glib-1.0' found
"gcc" -O0 -ggdb -Wall -fno-omit-frame-pointer  webrtc-sendrecv.c  -o webrtc-sendrecv
webrtc-sendrecv.c:9:10: fatal error: gst/gst.h: No such file or directory
 #include <gst/gst.h>
          ^~~~~~~~~~~
compilation terminated.
Makefile:6: recipe for target 'webrtc-sendrecv' failed
make: *** [webrtc-sendrecv] Error 1
````

This happens, because many of the required libraries have not been installed yet. Let's fix this:
````
> sudo apt-get install libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-doc gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-qt5 gstreamer1.0-pulseaudio
> sudo apt-get install libjson-glib-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev
````

Now running 'make' succeeds:
```
> make
"gcc" -O0 -ggdb -Wall -fno-omit-frame-pointer -pthread -I/usr/include/gstreamer-1.0 -I/usr/include/json-glib-1.0 -I/usr/include/libsoup-2.4 -I/usr/include/libxml2 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include webrtc-sendrecv.c -pthread -I/usr/include/gstreamer-1.0 -I/usr/include/json-glib-1.0 -I/usr/include/libsoup-2.4 -I/usr/include/libxml2 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -lgstsdp-1.0 -lgstwebrtc-1.0 -lgstbase-1.0 -lgstreamer-1.0 -ljson-glib-1.0 -lsoup-2.4 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -o webrtc-sendrecv
```

Open a browser and navigate here:

````
https://webrtc.nirbheek.in/
````

Check the peer ID from the web page, then run it like this:
````
> ./webrtc-sendrecv --peer-id 2920
````

You should get the bouncing ball again. Also video from my own webcam appeared on the Ubuntu desktop, showing live video view.

One last thing with the demo is to use the signaling server running on our own computer:
````
> ./webrtc-sendrecv --server wss://localhost:8443 --peer-id 2920
````

Now, the same demo appears on the Firefox browser running in the Ubuntu VM, showing the bouncing ball as well as the live webcam video stream.

## Using Finwe's Signaling Server component

GStreamer's WebRTC example sendrecv.c uses the Soup library (libsoup) for connecting to their signaling server, using WebSockets.

However, Finwe's SignalingServer implementation uses Socket.IO Node.js server for signaling. Socket.IO is not a pure WebSockets implementation; it is more like an advanced version running on top of WebSockets. From their documentation:

> Socket.IO is NOT a WebSocket implementation. Although Socket.IO indeed uses WebSocket as a transport when possible, it adds additional metadata to each packet. **That is why a WebSocket client will not be able to successfully connect to a Socket.IO server**, and a Socket.IO client will not be able to connect to a plain WebSocket server either.

This means that we can't just use Soup with a different server url to connect to Finwe's SignalingServer; we need to use Socket.IO compatible library.

GStreamer's WebRTC example sendrecv.c is written in C language. Socket.IO has a client library for 8 different programming languages, but not for C... However, there is a C++ version that we can use by creating some wrapping around it:

https://github.com/socketio/socket.io-client-cpp

### Socket.IO C++ client

Let's get the source code for the C++ client, compile and install it:
````
> cd ~/Source
> git clone --recurse-submodules https://github.com/socketio/socket.io-client-cpp.git
> cd cd socket.io-client-cpp
> sudo apt install cmake
> cmake ./
> sudo make install
````

This produces the following files:
````
/usr/local/include/sio_client.h
/usr/local/include/sio_message.h
/usr/local/include/sio_socket.h
/usr/local/lib/libsioclient.a
````

Let's test it:
````
> cd ~/Source/socket.io-client-cpp/examples/Console
> cmake ./
> sudo make install
````

This produces *sio_console_demo* executable, which we can run as follows:
````
> ./sio_console_demo 
[2021-06-17 13:59:20] [info] asio async_connect error: asio.system:111 (Connection refused)
[2021-06-17 13:59:20] [info] Error getting remote endpoint: asio.system:107 (Transport endpoint is not connected)
[2021-06-17 13:59:20] [error] handle_connect error: Connection refused
[2021-06-17 13:59:25] [info] asio async_connect error: asio.system:111 (Connection refused)
[2021-06-17 13:59:25] [info] Error getting remote endpoint: asio.system:107 (Transport endpoint is not connected)
````

We get connection errors, because main.cpp file has hardcoded URL on line 114 and we don't have a Socket.IO server running here.
````
h.connect("http://127.0.0.1:3000");
`````

The C++ client contains a simple echo server. We need to install and run it with Node.js, 
whose version is *at least 10* (newer than what Ubuntu 18.04 package manager provides):
````
> cd ~/Source/socket.io-client-cpp/test/echo_server
> sudo apt install curl
> curl -sL https://deb.nodesource.com/setup_12.x | sudo -E bash -
> sudo apt-get install -y nodejs
> node --version
v12.22.1
> npm install socket.io express --save
````

Let's run the echo server:
````
> node index.js
Listening on port 3000
````

Now, keep that running, and run the console demo again:
````
> cd ~/Source/socket.io-client-cpp/examples/Console
[2021-06-17 15:10:16] [connect] Successful connection
[2021-06-17 15:10:16] [connect] WebSocket Connection 127.0.0.1:3000 v-2 "WebSocket++/0.8.2" /socket.io/?EIO=4&transport=websocket&t=1623931816 101
Type your nickname:
````

Also, the signaling server has logged something:
````
Listening on port 3000
new connection
````

This means that we have successfully establised a connection from C++ code to (their example) Node.js Socket.IO server. Next task is to open a connection to Finwe's SignalingServer, running on another machine and using TLS/SSL.

#### C++ client and Finwe SignalingServer

The first thing is to edit the example app and change the server URL.
````
> cd ~/Source/socket.io-client-cpp/examples/Console
> nano main.cpp
````

Find line with "http://127.0.0.1:3000" and change it to "https://192.168.1.115:443/rtc/socket.io" (use the IP of the machine that is running Finwe's SignalingServer). After recompiling, we get:
````
./sio_console_demo 
[2021-06-17 14:05:52] [connect] Successful connection
[2021-06-17 14:05:52] [error] handle_read_http_response error: websocketpp.transport:7 (End of File)
[2021-06-17 14:05:57] [connect] Successful connection
[2021-06-17 14:05:57] [error] handle_read_http_response error: websocketpp.transport:7 (End of File)
[2021-06-17 14:06:04] [connect] Successful connection
````

The first problem seems to be that the Socket.IO C++ client was built without TLS/SSL support:
```
> cd ~/Source/socket.io-client-cpp/
> cmake ./
-- not define build type, set to release
-- Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR) 
-- Configuring done
-- Generating done
-- Build files have been written to: /home/finwedev/Source/socket.io-client-cpp
```

Install OpenSSL:
````
> sudo apt-get install openssl libssl-dev
````

Rebuild the client:
````
> cmake ./
-- not define build type, set to release
-- Found OpenSSL: /usr/lib/x86_64-linux-gnu/libcrypto.so (found version "1.1.1") 
-- Configuring done
-- Generating done
-- Build files have been written to: /home/finwedev/Source/socket.io-client-cpp
> sudo make install
````

Rebuild the example:
````
> cd ~/Source/socket.io-client-cpp/examples/Console
> cmake ./
> sudo make install
````

After build, the directory now contains both 'libsioclient.a' and 'libsioclient_tls.a'. Next we need to ensure that the example app uses the one with TLS support:
````
> nano CMakeLists.txt
````

Replace ```target_link_libraries(sio_console_demo sioclient)``` with ```target_link_libraries(sio_console_demo sioclient_tls)```. Save with CTRL+O and exit with CTRL+X.

Rebuild the example:
````
> cmake ./
> sudo make clean
> sudo make install
````

When you run the app, you should be able to connect to Finwe's SignalingServer:
````
./sio_console_demo 
[2021-06-17 18:49:19] [connect] Successful connection
[2021-06-17 18:49:19] [connect] WebSocket Connection 192.168.1.115:443 v-2 "WebSocket++/0.8.2" /rtc/socket.io?EIO=4&transport=websocket&t=1623944959 101
Type your nickname:
````
The SignalingServer's log should show this:
````
[15.49.19] Socket connected
[15.49.19] SEND: 'init': Server --> Connected socket
````

#### Tips for debugging

If you can't get the connection to work with Finwe' SignalingServer, first check with another Node.js server that TLS/SSL is working alright.

Here is a minimal Node.js server with HTTPS support (index.js):
````
const https = require('https')
const fs = require('fs');

const options = {
  key: fs.readFileSync('cert/server.key'),
  cert: fs.readFileSync('cert/server.crt')
};

https.createServer(options, function (req, res) {
  res.writeHead(200);
  res.end("hello world\n");
}).listen(443);
````

Before you can run it, copy SignalingServer's server.key and server.crt under /cert directory.

Then, you can run it like this with helpful debugging info:
```
sudo DEBUG=* node index.js
```

> NOTE: Getting TLS/SSL to work can be tricky. Here are a few helpful tools/tricks to test against secure servers:
>
> Check SSL: ```openssl s_client -connect localhost:443```
>
> Download page: ```curl -k "https://localhost" --verbose```
>
> Open in browser: https://localhost:443
>
> When TLS/SSL is working properly, all of these should work without issues.

Once this works OK, add Socket.IO support to the Node.js service; something like this (index.js):
````
const express = require('express');
const path = require('path');

var port = 443;

// Self signed certificate
const { PRIVATE_KEY, CERTIFICATE } = require('./cert/');
const credentials = { key: PRIVATE_KEY, cert: CERTIFICATE };

const app = express();
const server = require('https').createServer(credentials, app);
const io = require('socket.io')(server, { path: '/rtc' });
// above: with /rtc path, you need to connect like this:
// ./sio_console_demo https://192.168.1.115/rtc/socket.io

// Start server
server.listen(port, () => {
  console.log(`Server listening at port ${port}`);
});

// Static website hosting
app.use(express.static(path.join(__dirname, 'public')));

// Socket.IO events
io.on("connection", function(socket){
    console.log("new connection");
  });
````

This will publish whatever is in /public dir (put simple index.html file there) and you can test e.g. with Firefox. However, this also starts a Socket.IO service, which you can connect to from the C++ client:
````
./sio_console_demo https://192.168.1.115:443/rtc/socket.io
````

Here we have modified the C++ client's console example (main.c) to use command line parameter; testing is much easier this way:
````
    if (argc != 2) {
        h.connect("http://127.0.0.1:3000");
    } else {
        h.connect(args[1]);
    }
````

### Integrating Socket.IO C++ client to WebRTC sendrecv example

The sendrecv example is written in C but Socket IO client is written in C++. It is possible to create Gstreamer pipelines with C++, so let's do that and start with a basic example, as described in this tutorial:

https://medium.com/analytics-vidhya/creating-gstreamer-multimedia-pipeline-with-c-part-1-a7f0f86b5e1f

First, let's create a makefile for cmake:
````
> cd ~/Source/livesync-gstreamer/src/
> nano CMakeLists.txt
````

Copy-paste the following, then CTRL+O to save and CTRL+X to exit:
````
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

project(livesync_gstreamer) #Project name

set(CMAKE_CXX_STANDARD 14) #setting C++ 14 standard
find_package(PkgConfig) #finding pkg-config is a helper tool

#using pkg-config to getting Gstreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

#including GStreamer header files directory
include_directories(
        ${GLIB_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
)

#linking GStreamer library directory
link_directories(
        ${GLIB_LIBRARY_DIRS}
        ${GSTREAMER_LIBRARY_DIRS}
)

#building target executable
add_executable(${PROJECT_NAME} main.cpp)

#linking Gstreamer library with target executable
target_link_libraries(${PROJECT_NAME} ${GSTREAMER_LIBRARIES})

````

Create a simple Gstreamer pipeline with C++:
````
> nano main.cpp
````

Copy-paste the following, then CTRL+O to save and CTRL+X to exit:
````
#include <iostream>
#include <gst/gst.h>

int main(int arg, char *argv[]) {
    GstElement *pipeline = nullptr;
    GstBus *bus = nullptr;
    GstMessage *msg = nullptr;

    // gstreamer initialization
    gst_init(&arg, &argv);

    // building pipeline
    pipeline = gst_parse_launch(
            "playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm",
            nullptr);

    // start playing
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // wait until error or EOS ( End Of Stream )
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                     static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // free memory
    if (msg != nullptr)
        gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}
````

Compile and run the program:
```
> cmake ./
> make
> ./livesync_gstreamer
```

We are now running basically the same thing as basic-tutorial-1.c where we started with Gstreamer.

Next, use nano to replace CMakeLists.txt with a version that includes Socket.IO cpp client:
````
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

# First, build socket.io cpp client
include(${CMAKE_CURRENT_SOURCE_DIR}/../../socket.io-client-cpp/CMakeLists.txt)

# Project name
project(livesync_gstreamer)

# Set C++ 14 standard
set(CMAKE_CXX_STANDARD 14)

# Find pkg-config (a helper tool)
find_package(PkgConfig)

# Use pkg-config for getting Gstreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

# Include GStreamer header files directory
include_directories(
        ${GLIB_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
)

# Link GStreamer library directory
link_directories(
        ${GLIB_LIBRARY_DIRS}
        ${GSTREAMER_LIBRARY_DIRS}
)

# Build target executable
add_executable(${PROJECT_NAME} main.cpp)

# Link socket.io cpp library with target executable
target_link_libraries(${PROJECT_NAME} sioclient_tls)
target_link_libraries(${PROJECT_NAME} pthread)

# Link Gstreamer library with target executable
target_link_libraries(
        ${PROJECT_NAME} 
        ${GSTREAMER_LIBRARIES} 
)
````


Then, use nano to replace main.cpp contents with modified skeleton from the wertc-sendrecv.c example:
````
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
    // The original webrtc-sendrecv example uses libsoup to connect to a
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
````

Compile and run again:
````
> cmake ./
> make
> ./livesync_gstreamer --server https://192.168.1.115:443/rtc/socket.io
*** LiveSYNC Gstreamer example ***
Checking required plugins... OK
Checking required parameters... OK
Error: No active session
[2021-06-21 14:24:19] [connect] Successful connection
[2021-06-21 14:24:19] [connect] WebSocket Connection 192.168.1.115:443 v-2 "WebSocket++/0.8.2" /rtc/socket.io?EIO=4&transport=websocket&t=1624274659 101
Error: No active session
````

> NOTE: instead of 192.168.1.115, you need to use the IP address of the machine running Finwe' SignalServer docker container.

Also, in the SignalingServer's console, you should see something like this:
````
[11.24.19] Socket connected
[11.24.19] SEND: 'init': Server --> Connected socket
````

This means that we can now connect to Finwe's SignalingServer from a C/C++ codebase for Gstreamer WebRTC streaming pipeline. The next task is to add actual signaling messages and copy more code from the webrtc-sendrcv.c example to actually open WebRTC streams.

### Adding relevant signals (Socket.IO messages)

From now on, we won't document every code addition in this README file - check commits and commit messages from the GitHub repository for step-by-step instructions, if necessary.

Step 1: Connection handling and registering to signal server
- Added connection_listener class and cleanup_and_quit_loop function, so that we can observe connection state and cleanup when connection is closed
- Added json-glib to CMakeLists.txt, so that we can use JSON for in/out signals
- Added response_to_init(), so that we can properly respond to signaling server's 'init' request
- Modified connect_to_socketio_server_async() to use connection listener and handle 'init' request
- With these modifications, we successfully register to the signaling server, and close the connection after a timeout of inactivity

Step 2: Fix the ping-pong
- Currently, the connection will auto close after 30 seconds, which we of course don't want. Apparently, this is not just a missing configuration somewhere, but a version glitch between Socket.IO 2.x and 3.0: https://github.com/socketio/socket.io/issues/3698
- Finwe's SignalingServer is based on Socket.IO 2.3, but Socket.IO C++ client's master repo now contains 3.0 client and they say that is *not* compatible with 2.x (README.md in the repo).
- The branch 2.x should contain a compatible version, which we tried, but it couldn't connect with anything using TLS (seems that current status is broken in 2.x branch?).
- Nevertheless, there aren't much code changes between 2.0 release and 3.0 release, especially those that are related to ping-pong keepalive system. According to documentation here and there, they have reversed ping-pong direction somewhere between 2.x and 3.0. As a result, Finwe's SignalServer assumes that ping events (frame number '2') are sent periodically from the client, which can be confirmed working fine like that from our web player (Chrome's debug log). However, the C++ client does not send them as it now excepts *the server* to send them, hence it does not receive 'pong' replies either (frame number '3'), and decides to close the connection after 30 seconds... doh.
- To fix this, we can simply revert a few recent changes in the C++ client's code to make it send 'ping' events again and to handle 'pong' events that come from the SignalServer as a response:
````
> cd ~/Source/socket.io/socket.io-client-cpp/src/internal
> nano sio_packet.cpp
````
In client_impl::ping(), add this just above "if(!m_ping_timeout_timer)":
````
if(m_ping_timer)
        {
            asio::error_code e_code;
            m_ping_timer->expires_from_now(milliseconds(m_ping_interval), e_code);
            m_ping_timer->async_wait(std::bind(&client_impl::ping,this, std::placeholders::_1));
        }
````

In client_impl::on_handshake(), add this just above "return;":
````
m_ping_timer.reset(new asio::steady_timer(m_client.get_io_service()));
asio::error_code ec;
m_ping_timer->expires_from_now(milliseconds(m_ping_interval), ec);
if(ec)LOG("ec:"<<ec.message()<<endl){};
m_ping_timer->async_wait(std::bind(&client_impl::ping,this, std::placeholders::_1));
LOG("On handshake,sid:"<<m_sid<<",ping interval:"<<m_ping_interval<<",ping timeout"<<m_ping_timeout<<endl);
````

In client_impl::on_ping():
- change method name to client_impl::on_pong()
- remove this from the beginning of the method:
````
 packet p(packet::frame_pong);
        m_packet_mgr.encode(p, [&](bool /*isBin*/,shared_ptr<const string> payload)
        {
            this->m_client.send(this->m_con, *payload, frame::opcode::text);
        });
````

In client_impl::on_decode(), replace
````
case packet::frame_ping:
  this->on_ping();
````
with
````
case packet::frame_pong:
  this->on_pong();
````

In client_impl::clear_timers(), add this to the end of the method:
````
if(m_ping_timer)
{
    m_ping_timer->cancel(ec);
    m_ping_timer.reset();
}
````

Save (CTRL+O) and exit (CTRL+X).

Then, edit also the header file:
````
> nano sio_packet.h
````

Find "void on_ping();" and replace it with "void on_pong();".

Above "std::unique_ptr<asio::steady_timer> m_ping_timeout_timer;" add this:
````
std::unique_ptr<asio::steady_timer> m_ping_timer;
````
Save (CTRL+O) and exit (CTRL+X).

These changes were picked from here:
https://github.com/socketio/socket.io-client-cpp/compare/2.0.0...3.0.0

Now, let's recompile and test again:
```
> cd ~/Source/livesync-gstreamer/src
> cmake ./
> sudo make clean
> sudo make install
> ./livesync_gstreamer
```

And, lo and behold, the socket isn't closed anymore and from the SignalServer's end, we can see a ping event ("2") coming from the C++ client according to ping interval defined by the server, and a response ("3") being sent back to it.


Step 3: Bind to events and handle them
- 

