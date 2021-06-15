# app-livesync-gstreamer-hello

A tutorial project where a Gstreamer pipeline on Ubuntu receives a videostream from a 360 cam over WebRTC.

## Abstract

The purpose of this project is demonstrate how you can connect to a 360° camera and receive a low-latency video/audio stream over WebRTC.

As a starting point, we will use a 360° camera with Android operating system and Finwe's LiveSYNC Camera app installed to it, as well as Finwe's dockerized signaling server for messaging. We will build a simple Gstreamer pipeline on Ubuntu Linux host, and make it communicate with the camera via the signaling server using websockets, and receive a live videostream from it using WebRTC.

## Hardware

The following hardware is required:
1. Labpano Pilot or Era 360° camera
2. A PC for running Docker and Finwe's SignalingServer (desktop, laptop, or e.g. an NVidia Xavier box)
3. A PC (or a VM) for running Ubuntu Linux 18.04 and a Gstreamer pipeline we will build
4. Local Wifi/Ethernet network for communication

## Setup

1. Install Finwe's LiveSYNC camera app to Labpano Pilot or Era camera, if not already installed.
2. Install Finwe's SignalingServer (docker container) to a PC, if not already installed.
3. Configure LiveSYNC camera app via settings.ini file in the camera.




