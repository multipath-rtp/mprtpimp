/* GStreamer Mprtp sender
 * Copyright (C) 2008 Nokia Corporation. (contact <stefan.kost@nokia.com>)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-output-selector
 * @see_also: #GstMpRtcpReceiver
 *
 * Description
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include "gstmprtcpreceiver.h"
#include "mprtplibs.h"

#define GST_CAT_DEFAULT gst_mprtcp_receiver_debug

#define gst_mprtcp_receiver_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstMpRtcpReceiver, gst_mprtcp_receiver,
    GST_TYPE_ELEMENT, GST_DEBUG_CATEGORY_INIT (gst_mprtcp_receiver_debug, \
            "mprtcpreceiverer", 0, "MpRTCP Receiver"););

#define GST_MPRTCP_BUFFER_FOR_PACKETS(b,buffer,packet) \
  for ((b) = gst_rtcp_buffer_get_first_packet ((buffer), (packet)); (b); \
          (b) = gst_rtcp_packet_move_to_next ((packet)))


static GstStaticPadTemplate gst_rtcp_sink_factory =
GST_STATIC_PAD_TEMPLATE ("rtcp_sink",
    GST_PAD_SINK,
	GST_PAD_ALWAYS,
    GST_MPRTCP_RR_CAPS);

static GstStaticPadTemplate gst_rtcp_src_factory =
GST_STATIC_PAD_TEMPLATE ("rtcp_src",
		GST_PAD_SRC,
	GST_PAD_ALWAYS,
    GST_MPRTCP_RR_CAPS);

static GstStaticPadTemplate gst_mprtcp_src_factory =
GST_STATIC_PAD_TEMPLATE ("mprtcp_src",
    GST_PAD_SRC,
	GST_PAD_ALWAYS,
    GST_MPRTCP_RR_CAPS);



//---------------------------------------------------------------
//-------------------- LOCAL TYPE DEFINITIONS ------------------
//---------------------------------------------------------------

enum
{
  PROP_0,
};

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};


static void gst_mprtcp_receiver_dispose (GObject * object);
static void gst_mprtcp_receiver_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_mprtcp_receiver_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_mprtcp_receiver_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
static GstStateChangeReturn gst_mprtcp_receiver_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_mprtcp_receiver_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_mprtcp_receiver_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static void
gst_mprtcp_receiver_class_init (GstMpRtcpReceiverClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->dispose = gst_mprtcp_receiver_dispose;

  gobject_class->set_property = gst_mprtcp_receiver_set_property;
  gobject_class->get_property = gst_mprtcp_receiver_get_property;

  gst_element_class_set_static_metadata (gstelement_class, "MpRTCP Receiver",
      "Generic", "Multipath RTCP Receiver for RTCP and MPRTCP packets",
      "Balázs Kreith <balazs.kreith@gmail.com>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtcp_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtcp_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_mprtcp_src_factory));

  gstelement_class->change_state = gst_mprtcp_receiver_change_state;


}

static void
gst_mprtcp_receiver_init (GstMpRtcpReceiver * mprtcpr)
{

  mprtcpr->sinkpad =
	  gst_pad_new_from_static_template (&gst_rtcp_sink_factory,
	  "rtcp_sink");
  gst_pad_set_chain_function (mprtcpr->sinkpad,
	  GST_DEBUG_FUNCPTR (gst_mprtcp_receiver_chain));
  gst_pad_set_event_function (mprtcpr->sinkpad,
	  GST_DEBUG_FUNCPTR (gst_mprtcp_receiver_event));
  gst_pad_set_query_function (mprtcpr->sinkpad,
	  GST_DEBUG_FUNCPTR (gst_mprtcp_receiver_query));

  GST_OBJECT_FLAG_SET (mprtcpr->sinkpad, GST_PAD_FLAG_PROXY_CAPS);
  gst_element_add_pad (GST_ELEMENT (mprtcpr), mprtcpr->sinkpad);

  mprtcpr->mprtcp_srcpad =
  	gst_pad_new_from_static_template (&gst_mprtcp_src_factory,
  	"mprtcp_src");
    gst_pad_use_fixed_caps (mprtcpr->mprtcp_srcpad);
    gst_pad_set_active (mprtcpr->mprtcp_srcpad, TRUE);
    gst_element_add_pad (GST_ELEMENT_CAST (mprtcpr), mprtcpr->mprtcp_srcpad);

  mprtcpr->rtcp_srcpad =
  	gst_pad_new_from_static_template (&gst_rtcp_src_factory,
  	"rtcp_src");
    gst_pad_use_fixed_caps (mprtcpr->rtcp_srcpad);
    gst_pad_set_active (mprtcpr->rtcp_srcpad, TRUE);
    gst_element_add_pad (GST_ELEMENT_CAST (mprtcpr), mprtcpr->rtcp_srcpad);

}



static void
gst_mprtcp_receiver_dispose (GObject * object)
{
  GstMpRtcpReceiver *mprtcpr = GST_MPRTCP_RECEIVER (object);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_mprtcp_receiver_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  //GstMpRtcpReceiver *mprtcpr = GST_MPRTCP_RECEIVER (object);
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mprtcp_receiver_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  //GstMpRtcpReceiver *mprtcpr = GST_MPRTCP_RECEIVER (object);
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_mprtcp_receiver_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
	GstMpRtcpReceiver *mprtcpr;
	GstRTCPBuffer rtcp = {NULL, };
	GstBuffer *outbuf;
	GstPad *outpad;
	gboolean more;
	GstRTCPPacket packet;

	if(!gst_rtcp_buffer_map (buf, GST_MAP_READ, &rtcp)){
		GST_WARNING("The RTCP buffer is not readable");
		return GST_FLOW_ERROR;
	}

	mprtcpr = GST_MPRTCP_RECEIVER (parent);
	GST_OBJECT_LOCK (GST_OBJECT (mprtcpr));

	GST_MPRTCP_BUFFER_FOR_PACKETS (more, &rtcp, &packet); {
	/* first packet must be SR or RR or else the validate would have failed */
	switch (gst_rtcp_packet_get_type (&packet)) {
	  case MPRTCP_PACKET_TYPE_IDENTIFIER:
		//we extend the sender reports by adding the subflow informations.
		GST_LOG_OBJECT(mprtcpr, "MPRTCP packet is received and going to be processed and forwards to MPRTCP and RTCP packets.");
		break;
	  default:
		break;
	}
	}
	gst_rtcp_buffer_unmap (&rtcp);
	GST_OBJECT_UNLOCK(GST_OBJECT (mprtcpr));

	if (!gst_pad_is_linked (mprtcpr->rtcp_srcpad)) {
	  GST_ERROR_OBJECT(mprtcpr, "The MPRTCP sink is connected, but src is not");
	  return GST_FLOW_ERROR;
	}

	GST_LOG_OBJECT (mprtcpr, "pushing rtcp buffer to mprtcp srcpad");
	return gst_pad_push (mprtcpr->rtcp_srcpad, outbuf);
}

static GstStateChangeReturn
gst_mprtcp_receiver_change_state (GstElement * element,
    GstStateChange transition)
{
  GstMpRtcpReceiver *sel;
  GstStateChangeReturn result;

  sel = GST_MPRTCP_RECEIVER (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  return result;
}

static gboolean
gst_mprtcp_receiver_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMpRtcpReceiver *mprtcpr;
  //GstPad *active = NULL;

  mprtcpr = GST_MPRTCP_RECEIVER (parent);
  return gst_pad_push_event(mprtcpr->rtcp_srcpad, event) &&
		  gst_pad_push_event(mprtcpr->mprtcp_srcpad, event);
}

static gboolean
gst_mprtcp_receiver_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
	GstMpRtcpReceiver *mprtcpr;
	//GstPad *active = NULL;

	mprtcpr = GST_MPRTCP_RECEIVER (parent);
	return gst_pad_peer_query(mprtcpr->rtcp_srcpad, query) &&
			gst_pad_peer_query(mprtcpr->mprtcp_srcpad, query);
}



