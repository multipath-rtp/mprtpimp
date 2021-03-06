#!/bin/sh
#
# A simple RTP server 
#  sends the output of videotestsrc as h263+ encoded RTP on port 5000, RTCP is sent on
#  port 5001. The destination is 127.0.0.1.
#  the video receiver RTCP reports are received on port 5005
#
# .-------.    .-------.    .-------.      .----------.     .-------.
# |vts    |    |h263enc|    |h263pay|      | rtpbin   |     |udpsink|  RTP
# |      src->sink    src->sink    src->send_rtp send_rtp->sink     | port=5000
# '-------'    '-------'    '-------'      |          |     '-------'
#                                          |          |      
#                                          |          |     .-------.
#                                          |          |     |udpsink|  RTCP
#                                          |    send_rtcp->sink     | port=5001
#                           .-------.      |          |     '-------' sync=false
#                RTCP       |udpsrc |      |          |               async=false
#              port=5005    |     src->recv_rtcp      |                       
#                           '-------'      '----------'              
#

# change this to send the RTP data and RTCP to another host
DEST=127.0.0.1
DEST2=127.0.0.2

# tuning parameters to make the sender send the streams out of sync. Can be used
# ot test the client RTCP synchronisation. 
#VOFFSET=900000000
VOFFSET=0
AOFFSET=0

# H263+ encode from the source
VELEM="videotestsrc is-live=1"
VCAPS="video/x-raw,width=352,height=288,framerate=15/1"
VSOURCE="$VELEM ! $VCAPS"
VENC="avenc_h263p ! rtph263ppay"

VRTPSINK="udpsink port=5000 host=$DEST ts-offset=$VOFFSET async=false sync=false name=vrtpsink"
VRTPSINK2="udpsink port=5010 host=$DEST2 ts-offset=$VOFFSET async=false sync=false name=vrtpsink2"
VRTCPSINK="udpsink port=5001 host=$DEST sync=false async=false name=vrtcpsink"
VRTCPSRC="udpsrc port=5005 name=vrtpsrc"

PIPELINE="mprtpsender name=mprtps rtpbin name=rtpbin
            $VSOURCE ! $VENC ! rtpbin.send_rtp_sink_2
	      rtpbin.send_rtp_src_2 ! mprtps.mprtp_sink mprtps.mprtp_src_1 ! $VRTPSINK 
              rtpbin.send_rtcp_src_2 ! mprtps.rtcp_sink mprtps.mprtcp_src ! $VRTCPSINK
            $VRTCPSRC ! rtpbin.recv_rtcp_sink_2"

echo $PIPELINE
#--gst-fatal-warnings
gst-launch-1.0 --gst-debug-level=4  --gst-debug-no-color $PIPELINE
