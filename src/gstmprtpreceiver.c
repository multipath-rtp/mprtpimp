/* GStreamer Mprtp receiver
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
 * @see_also: #GstMprtpReceiver
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

#include "gstmprtpreceiver.h"
#include "mprtplibs.h"

#define GST_CAT_DEFAULT gst_mprtp_receiver_debug

#define gst_mprtp_receiver_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstMprtpReceiver, gst_mprtp_receiver,
    GST_TYPE_ELEMENT, GST_DEBUG_CATEGORY_INIT (gst_mprtp_receiver_debug, \
            "mprtpreceiver", 0, "MpRtp Receiver"););


static GstStaticPadTemplate gst_mprtp_receiver_sink_factory =
GST_STATIC_PAD_TEMPLATE ("mprtp_sink_%u",
    GST_PAD_SINK,
	GST_PAD_REQUEST,
    GST_MPRTP_PAD_CAPS);

static GstStaticPadTemplate gst_mprtp_receiver_src_factory =
GST_STATIC_PAD_TEMPLATE ("mprtp_src",
    GST_PAD_SRC,
	GST_PAD_ALWAYS,
    GST_MPRTP_PAD_CAPS);

static GstStaticPadTemplate gst_mprtcp_receiver_src_factory =
GST_STATIC_PAD_TEMPLATE ("mprtcp_src",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_MPRTCP_SR_CAPS);


//---------------------------------------------------------------
//-------------------- LOCAL TYPE DEFINITIONS ------------------
//---------------------------------------------------------------

typedef struct _Subflow Subflow;

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

struct _Subflow{
	guint16 id;
    guint16 seq_num;
    guint16 sent;
    GstPad* srcpad;
};


static void gst_mprtp_receiver_dispose (GObject * object);
static void gst_mprtp_receiver_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_mprtp_receiver_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstPad *gst_mprtp_receiver_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * unused, const GstCaps * caps);
static void gst_mprtp_receiver_release_pad (GstElement * element,
    GstPad * pad);
static GstFlowReturn gst_mprtp_receiver_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
static GstStateChangeReturn gst_mprtp_receiver_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_mprtp_receiver_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_mprtp_receiver_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static void _subflow_dtor(Subflow* target);
static void _dispose_subflow(Subflow* target);
static Subflow* _make_subflow(guint16 id, GstPad* srcpad);
static Subflow* _subflow_ctor();

static void
gst_mprtp_receiver_class_init (GstMprtpReceiverClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->dispose = gst_mprtp_receiver_dispose;

  gobject_class->set_property = gst_mprtp_receiver_set_property;
  gobject_class->get_property = gst_mprtp_receiver_get_property;

  gst_element_class_set_static_metadata (gstelement_class, "MpRtp receiver",
      "Generic", "Multipath Rtp protocol interpreter for receiver part",
      "Balázs Kreith <balazs.kreith@gmail.com>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_mprtp_receiver_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&gst_mprtp_receiver_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&gst_mprtcp_receiver_src_factory));

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_mprtp_receiver_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_mprtp_receiver_release_pad);

  gstelement_class->change_state = gst_mprtp_receiver_change_state;


}

static void
gst_mprtp_receiver_init (GstMprtpReceiver * mprtpr)
{

  mprtpr->srcpad = gst_pad_new_from_static_template (
		  &gst_mprtp_receiver_src_factory, "mprtp_src");

  gst_element_add_pad (GST_ELEMENT (mprtpr), mprtpr->srcpad);

  /* sinkpad management */
  mprtpr->subflows = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_free);
  mprtpr->RTPBuffer = (GstRTPBuffer) GST_RTP_BUFFER_INIT;
}

static void
gst_mprtp_receiver_reset (GstMprtpReceiver * mprtpr)
{

}

static void _mprtp_receiver_subflow_dispose(gpointer key, gpointer value, gpointer user_data) {
	Subflow *subflow = (Subflow*) value;
	//unref something...
}

static void
gst_mprtp_receiver_dispose (GObject * object)
{
  GstMprtpReceiver *mprtpr = GST_MPRTP_RECEIVER (object);

  g_hash_table_foreach(mprtpr->subflows,
		  (GHFunc)_mprtp_receiver_subflow_dispose, NULL);
  g_hash_table_destroy(mprtpr->subflows);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_mprtp_receiver_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  //GstMprtpReceiver *mprtpr = GST_MPRTP_RECEIVER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mprtp_receiver_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  //GstMprtpReceiver *mprtpr = GST_MPRTP_RECEIVER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
forward_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GstPad *srcpad = GST_PAD_CAST (user_data);

  gst_pad_push_event (srcpad, gst_event_ref (*event));

  return TRUE;
}


static GstPad* _request_mprtp_sinkpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
GstPad* _request_mprtp_sinkpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
	GstPad *sinkpad;
	GstMprtpReceiver *mprtpr;
	Subflow* subflow;
	guint16 *subflow_id;
g_print("----Adding pad\n");
	mprtpr = GST_MPRTP_RECEIVER (element);
	GST_OBJECT_LOCK (mprtpr);

	subflow_id = g_new0(guint16, 1);
	sscanf(name, "mprtp_sink_%hu", subflow_id);
	sinkpad = gst_pad_new_from_template (templ, name);
	subflow = _make_subflow(subflow_id, sinkpad);
	g_hash_table_insert(mprtpr->subflows, subflow_id, subflow);

	GST_OBJECT_UNLOCK (mprtpr);

	/* Forward sticky events to the new sinkpad */
	gst_pad_set_event_function (sinkpad,
	    GST_DEBUG_FUNCPTR (gst_mprtp_receiver_event));
    gst_pad_set_query_function (sinkpad,
	    GST_DEBUG_FUNCPTR (gst_mprtp_receiver_query));
    gst_pad_set_chain_function (sinkpad,
	    GST_DEBUG_FUNCPTR (gst_mprtp_receiver_chain));
    //gst_pad_set_iterate_internal_links_function (srcpad,
	  //  GST_DEBUG_FUNCPTR (gst_selector_pad_iterate_linked_pads));

    GST_OBJECT_FLAG_SET (sinkpad, GST_PAD_FLAG_PROXY_CAPS);
    GST_OBJECT_FLAG_SET (sinkpad, GST_PAD_FLAG_PROXY_ALLOCATION);
    gst_pad_set_active (sinkpad, TRUE);

    gst_element_add_pad (GST_ELEMENT (mprtpr), sinkpad);
g_print("Pad %s is added", name);
	return sinkpad;
}

static GstPad* _create_mprtcp_sr_sinkpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
GstPad* _create_mprtcp_sr_sinkpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{

	return NULL;
}

static GstPad* _create_mprtcp_rr_srcpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
GstPad* _create_mprtcp_rr_srcpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
	return NULL;
}

static GstPad *
gst_mprtp_receiver_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *result = NULL;
  GstElementClass *klass;
  GstMprtpReceiver *mprtpr;
  mprtpr = GST_MPRTP_RECEIVER (element);
  klass = GST_ELEMENT_GET_CLASS (element);
  g_print("R::::%s", templ->name_template);
  if(templ == gst_element_class_get_pad_template (klass, "mprtp_sink_%u")){
	  result = _request_mprtp_sinkpad(element, templ, name, caps);
  }else if(templ == gst_element_class_get_pad_template (klass, "mprtcprr_src")){

  }else if(templ == gst_element_class_get_pad_template (klass, "mprtcpsr_sink")){

  }

  return result;
}

static void
gst_mprtp_receiver_release_pad (GstElement * element, GstPad * pad)
{
  GstMprtpReceiver *mprtpr;

  mprtpr = GST_MPRTP_RECEIVER (element);

  GST_DEBUG_OBJECT (mprtpr, "releasing pad");

  gst_pad_set_active (pad, FALSE);

  gst_element_remove_pad (GST_ELEMENT_CAST (mprtpr), pad);
}


static void _print_rtp_packet_info(GstRTPBuffer *rtp)
{
	g_print(
   "0               1               2               3          \n"
   "0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 \n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%3d|%1d|%1d|%7d|%1d|%13d|%31d|\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%63lu|\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%63lu|\n"
   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
   "|       0xBE    |    0xDE       |           length=N-1          |\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|   ID  |  LEN  |  MPID |LENGTH |       MPRTP header data       |\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +\n"
   "|                             ....                              |\n"
   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
   "|                         RTP payload                           |\n"
   "|                             ....                              |\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n",
			gst_rtp_buffer_get_version(rtp),
			gst_rtp_buffer_get_padding(rtp),
			gst_rtp_buffer_get_extension(rtp),
			gst_rtp_buffer_get_csrc_count(rtp),
			gst_rtp_buffer_get_marker(rtp),
			gst_rtp_buffer_get_payload_type(rtp),
			gst_rtp_buffer_get_seq(rtp),
			gst_rtp_buffer_get_timestamp(rtp),
			gst_rtp_buffer_get_ssrc(rtp)
			);
}

static void _processing_mprtp_packet(GstMprtpReceiver *mprtpr, GstBuffer * buf)
{
	GstRTPBuffer *rtp = &mprtpr->RTPBuffer;
	if (G_UNLIKELY (!gst_rtp_buffer_map(buf, GST_MAP_READ, rtp))){
		return;
	}

	_print_rtp_packet_info(rtp);
	gst_rtp_buffer_unmap(rtp);
}

static GstFlowReturn
gst_mprtp_receiver_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn result;
  GstMprtpReceiver *mprtpr;
  //GstClockTime position, duration;

  mprtpr = GST_MPRTP_RECEIVER (parent);
  _processing_mprtp_packet(mprtpr, buf);
  result = gst_pad_push (mprtpr->srcpad, buf);

  return result;
}

static GstStateChangeReturn
gst_mprtp_receiver_change_state (GstElement * element,
    GstStateChange transition)
{
  GstMprtpReceiver *sel;
  GstStateChangeReturn result;

  sel = GST_MPRTP_RECEIVER (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_mprtp_receiver_reset (sel);
      break;
    default:
      break;
  }

  return result;
}


static gboolean
gst_mprtp_receiver_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = TRUE;
  GstMprtpReceiver *mprtpr;
  GstPad *active = NULL;

  mprtpr = GST_MPRTP_RECEIVER (parent);

  switch (GST_EVENT_TYPE (event)) {
  	  case GST_EVENT_CAPS:
      {
        GstCaps * caps;

        gst_event_parse_caps (event, &caps);
        /* do something with the caps */

        /* and forward */
        res = gst_pad_event_default (pad, parent, event);
        break;
      }
      default:
    	  res = gst_pad_event_default (pad, parent, event);
        break;
  }

  return res;
}

static gboolean
gst_mprtp_receiver_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = TRUE;
//  GstMprtpReceiver *sel;
//  GstPad *active = NULL;

//  sel = GST_MPRTP_RECEIVER (parent);

  switch (GST_QUERY_TYPE (query)) {
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

  return res;
}


//---------------------------------------------------------------
//-------- PRIVATE FUNCTIONS DEFINITIONS ------------------------
//---------------------------------------------------------------

Subflow* _subflow_ctor()
{
	Subflow* result;
	result = (Subflow*) g_malloc0(sizeof(Subflow));
	return result;
}

Subflow* _make_subflow(guint16 id, GstPad* srcpad)
{
	Subflow* result = _subflow_ctor();
	result->id = id;
	result->srcpad = srcpad;
	return result;
}

void _dispose_subflow(Subflow* target)
{

}

void _subflow_dtor(Subflow* target)
{
	g_free(target);
}

