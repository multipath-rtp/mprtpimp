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
 * @see_also: #GstMprtpSender
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
#include "gstmprtpsender.h"
#include "mprtplibs.h"

#define GST_CAT_DEFAULT gst_mprtp_sender_debug

#define gst_mprtp_sender_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstMprtpSender, gst_mprtp_sender,
    GST_TYPE_ELEMENT, GST_DEBUG_CATEGORY_INIT (gst_mprtp_sender_debug, \
            "mprtpsender", 0, "MpRtp Sender"););


static GstStaticPadTemplate gst_mprtp_sender_sink_factory =
GST_STATIC_PAD_TEMPLATE ("mprtp_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_MPRTP_PAD_CAPS);

static GstStaticPadTemplate gst_mprtp_sender_src_factory =
GST_STATIC_PAD_TEMPLATE ("mprtp_src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_MPRTP_PAD_CAPS);

static GstStaticPadTemplate gst_mprtcp_sender_src_factory =
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

typedef enum
{
  MPRTP_SUBFLOW_CONGESTED       = 1,
  MPRTP_SUBFLOW_NON_CONGESTED   = 2,
  MPRTP_SUBFLOW_MIDLY_CONGESTED = 3
}Congestion;

struct _Subflow{
	guint16 id;
    guint16 seq_num;
    guint16 sent;
    GstPad* srcpad;
    guint32 bw;
    Congestion congestion;
};


struct _SchNode
{
	SchNode *left, *right, *next;
	Subflow* value;
};

static void gst_mprtp_sender_dispose (GObject * object);
static void gst_mprtp_sender_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_mprtp_sender_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstPad *gst_mprtp_sender_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * unused, const GstCaps * caps);
static void gst_mprtp_sender_release_pad (GstElement * element,
    GstPad * pad);
static GstFlowReturn gst_mprtp_sender_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
static GstStateChangeReturn gst_mprtp_sender_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_mprtp_sender_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_mprtp_sender_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstPad* gst_mprtp_sender_switch (GstMprtpSender * mprtps);

static void _subflow_dtor(Subflow* target);
static void _dispose_subflow(Subflow* target);
static Subflow* _make_subflow(guint16 id, GstPad* srcpad);
static Subflow* _subflow_ctor();

static SchNode* _schnode_ctor();
static void _schnode_rdtor(SchNode* target);
static SchNode* _schtree_insert(Subflow* value,
		SchNode *node, guint32* target, guint32 current);
static SchNode* _schtree_setnext(SchNode* root);
static SchNode* _schtree_build(GSList* subflows, Congestion congestion);
static void _refresh_schtree(GstMprtpSender* mprtps);
void _recalculate_congestion(GstMprtpSender* mprtps);

static void
gst_mprtp_sender_class_init (GstMprtpSenderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->dispose = gst_mprtp_sender_dispose;

  gobject_class->set_property = gst_mprtp_sender_set_property;
  gobject_class->get_property = gst_mprtp_sender_get_property;

  gst_element_class_set_static_metadata (gstelement_class, "MpRtp sender",
      "Generic", "Multipath Rtp protocol interpreter for sender part",
      "Balázs Kreith <balazs.kreith@gmail.com>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_mprtp_sender_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&gst_mprtp_sender_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&gst_mprtcp_sender_src_factory));

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_mprtp_sender_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_mprtp_sender_release_pad);

  gstelement_class->change_state = gst_mprtp_sender_change_state;


}

static void
gst_mprtp_sender_init (GstMprtpSender * mprtps)
{

  mprtps->sinkpad =
      gst_pad_new_from_static_template (&gst_mprtp_sender_sink_factory,
      "mprtp_sink");
  gst_pad_set_chain_function (mprtps->sinkpad,
      GST_DEBUG_FUNCPTR (gst_mprtp_sender_chain));
  gst_pad_set_event_function (mprtps->sinkpad,
      GST_DEBUG_FUNCPTR (gst_mprtp_sender_event));
  gst_pad_set_query_function (mprtps->sinkpad,
      GST_DEBUG_FUNCPTR (gst_mprtp_sender_query));

  GST_OBJECT_FLAG_SET (mprtps->sinkpad, GST_PAD_FLAG_PROXY_CAPS);
  gst_element_add_pad (GST_ELEMENT (mprtps), mprtps->sinkpad);

  /* srcpad management */
  mprtps->active_srcpad = NULL;
  gst_segment_init (&mprtps->segment, GST_FORMAT_UNDEFINED);
  mprtps->subflows = NULL;
  GstRTPBuffer rtpbuf_init =  GST_RTP_BUFFER_INIT;
  mprtps->RTPBuffer = rtpbuf_init;


}

static void
gst_mprtp_sender_reset (GstMprtpSender * mprtps)
{
  GSList *it;
  Subflow *subflow;
  for(it = mprtps->subflows; it != NULL; it = it->next){
	  subflow = (Subflow*) it->data;
	  gst_object_unref(subflow);
  }
  g_slist_free(mprtps->subflows);
  gst_object_unref(mprtps->active_srcpad);
  gst_segment_init (&mprtps->segment, GST_FORMAT_UNDEFINED);
}

static void
gst_mprtp_sender_dispose (GObject * object)
{
  GstMprtpSender *mprtps = GST_MPRTP_SENDER (object);

  gst_mprtp_sender_reset (mprtps);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_mprtp_sender_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  //GstMprtpSender *mprtps = GST_MPRTP_SENDER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mprtp_sender_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  //GstMprtpSender *mprtps = GST_MPRTP_SENDER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstPad *
gst_mprtp_sender_get_active (GstMprtpSender * mprtps)
{
  GstPad *active = NULL;

  GST_OBJECT_LOCK (mprtps);
  if (mprtps->active_srcpad == NULL)
	  active = gst_mprtp_sender_switch(mprtps);
  if(active == NULL){
	  goto mprtp_sender_get_active_done;
  }
  active = gst_object_ref (mprtps->active_srcpad);

mprtp_sender_get_active_done:
  GST_OBJECT_UNLOCK (mprtps);
  return active;
}
static gboolean
forward_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GstPad *srcpad = GST_PAD_CAST (user_data);

  gst_pad_push_event (srcpad, *event);

  return TRUE;
}


static GstPad* _request_mprtp_srcpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
GstPad* _request_mprtp_srcpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
	GstPad *srcpad;
	GstMprtpSender *mprtps;
	Subflow* subflow;
	guint16 subflow_id;

	mprtps = GST_MPRTP_SENDER (element);

	GST_DEBUG_OBJECT (mprtps, "requesting pad");
	GST_OBJECT_LOCK (mprtps);

	sscanf(name, "mprtp_src_%hu", &subflow_id);
	srcpad = gst_pad_new_from_template (templ, name);
	subflow = _make_subflow(subflow_id, srcpad);
	mprtps->subflows = g_slist_append(mprtps->subflows, subflow);

	GST_OBJECT_UNLOCK (mprtps);
	gst_pad_set_active (srcpad, TRUE);

	/* Forward sticky events to the new srcpad */
	gst_pad_sticky_events_foreach (srcpad, forward_sticky_events, srcpad);

	gst_element_add_pad (GST_ELEMENT (mprtps), srcpad);

	if(mprtps->active_srcpad == NULL){
		mprtps->active_srcpad = srcpad;
	}
	return srcpad;
}

static GstPad* _create_mprtcp_rr_sinkpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
GstPad* _create_mprtcp_rr_sinkpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
	return NULL;
}

static GstPad* _create_mprtcp_sr_srcpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
GstPad* _create_mprtcp_sr_srcpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
	return NULL;
}

static GstPad *
gst_mprtp_sender_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *result = NULL;
  GstElementClass *klass;
  GstMprtpSender *mprtps;

  mprtps = GST_MPRTP_SENDER (element);
  klass = GST_ELEMENT_GET_CLASS (element);
  g_print("Requested pad name: %s", name);
  if(templ == gst_element_class_get_pad_template (klass, "mprtp_src_%u")){
	  result = _request_mprtp_srcpad(element, templ, name, caps);
  }else if(templ == gst_element_class_get_pad_template (klass, "mprtcpsr_src")){

  }else if(templ == gst_element_class_get_pad_template (klass, "mprtcprr_sink")){

  }

  if(result == NULL){
	  GST_WARNING_OBJECT (mprtps, "add failed, pad not linked");
	  //warning
  }
  return result;
}

static void
gst_mprtp_sender_release_pad (GstElement * element, GstPad * pad)
{
  GstMprtpSender *mprtps;

  mprtps = GST_MPRTP_SENDER (element);

  GST_DEBUG_OBJECT (mprtps, "releasing pad");

  gst_pad_set_active (pad, FALSE);

  gst_element_remove_pad (GST_ELEMENT_CAST (mprtps), pad);
}


GstPad* gst_mprtp_sender_switch (GstMprtpSender * mprtps)
{
  GstPad *res;
  //GstEvent *ev = NULL;
  //GstSegment *seg = NULL;

  /* Switch */
  //GST_OBJECT_LOCK (GST_OBJECT (mprtps));
  GST_INFO_OBJECT (mprtps, "switching to pad");
  res = mprtps->active_srcpad;

//  if (gst_pad_is_linked (mprtps->subflows)) {
//    mprtps->active_srcpad = mprtps->subflows;
//    res = TRUE;
//  }
//  mprtps->subflows = NULL;
//  GST_OBJECT_UNLOCK (GST_OBJECT (mprtps));
//
//  /* Send SEGMENT event and latest buffer if switching succeeded
//   * and we already have a valid segment configured */
//  if (res) {
//    gst_pad_sticky_events_foreach (mprtps->sinkpad, forward_sticky_events,
//        mprtps->active_srcpad);
//
//    /* update segment if required */
//    if (mprtps->segment.format != GST_FORMAT_UNDEFINED) {
//      /* Send SEGMENT to the pad we are going to switch to */
//      seg = &mprtps->segment;
//      ev = gst_event_new_segment (seg);
//
//      if (!gst_pad_push_event (mprtps->active_srcpad, ev)) {
//        GST_WARNING_OBJECT (mprtps,
//            "newsegment handling failed in %" GST_PTR_FORMAT,
//            mprtps->active_srcpad);
//      }
//    }
//  } else {
//    GST_WARNING_OBJECT (mprtps, "switch failed, pad not linked");
//  }

  return res;
}
#if G_BYTE_ORDER == G_LITTLE_ENDIAN

#elif G_BYTE_ORDER == G_BIG_ENDIAN

#endif

typedef struct _MpRtpHeaderExtension MpRtpHeaderExtension;
struct _MpRtpHeaderExtension{
	guint16  BEDE;
	guint16  length;
};

static void _print_rtp_packet_info(GstRTPBuffer *rtp)
{
	gboolean extended;
	gint index;
	guint32 *word;
	gpointer payload;
	g_print(
   "0               1               2               3          \n"
   "0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 \n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%3d|%1d|%1d|%7d|%1d|%13d|%31d|\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%63lu|\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%63lu|\n",
			gst_rtp_buffer_get_version(rtp),
			gst_rtp_buffer_get_padding(rtp),
			extended = gst_rtp_buffer_get_extension(rtp),
			gst_rtp_buffer_get_csrc_count(rtp),
			gst_rtp_buffer_get_marker(rtp),
			gst_rtp_buffer_get_payload_type(rtp),
			gst_rtp_buffer_get_seq(rtp),
			gst_rtp_buffer_get_timestamp(rtp),
			gst_rtp_buffer_get_ssrc(rtp)
			);

	if(extended){
	g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|       0xBE    |    0xDE       |           length=N-1          |\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|   ID  |  LEN  |  MPID |LENGTH |       MPRTP header data       |\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +\n"
	   "|                             ....                              |\n"

	   );
	}

	payload = gst_rtp_buffer_get_payload(rtp);
	g_print("+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n");
	for(index = 0; index < 400; ++index){
		word = (guint32*) payload + (index * 4);
		g_print("%X", word);
	}
	g_print("+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n");

}

static GstBuffer* _extending_buffer(GstMprtpSender* mprtps, GstBuffer* buf);
GstBuffer* _extending_buffer(GstMprtpSender* mprtps, GstBuffer* buf)
{
	GstRTPBuffer old_rtp = GST_RTP_BUFFER_INIT;
	GstRTPBuffer new_rtp = GST_RTP_BUFFER_INIT;
	GstBuffer *outbuf;
	gst_rtp_buffer_map(outbuf, GST_MAP_READ, &old_rtp);
	outbuf = gst_buffer_make_writable (buf);
	if (G_UNLIKELY (!gst_rtp_buffer_map(outbuf, GST_MAP_READWRITE, &new_rtp))){
		return buf;
	}
	gst_rtp_buffer_set_extension(&new_rtp, 1);
	gst_rtp_buffer_set_packet_len(new_rtp, gst_rtp_buffer_get_payload_len(old_rtp));

	gst_rtp_buffer_unmap(&new_rtp);
	gst_rtp_buffer_unmap(&old_rtp);
g_print("I am in");
	return buf;
	//gst_rtp_buffer_unmap(&mprtps->RTPBuffer);
}



/* Copy fixed header and extension. Add OSN before to copy payload
 * Copy memory to avoid to manually copy each rtp buffer field.
 */
static GstBuffer *
gst_mprtp_buffer_new (GstMprtpSender * mprtps, GstBuffer * buffer)
{
  GstMemory *mem = NULL;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstRTPBuffer new_rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *new_buffer = gst_buffer_new ();
  GstMapInfo map;
  guint payload_len = 0;
  guint32 ssrc;
  guint16 seqnum;
  guint8 fmtp;
  gint8 index;
  gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp);
  /* 0 - copy fixed header */
  /* 1 - copy extensions */
  /* 2 - copy payloads */
  for(index = 0; index < 4; ++index){
	if(rtp.size[index] < 1){
		continue;
	}
    mem = gst_memory_copy (rtp.map[index].memory, 0, rtp.size[index]);
    gst_buffer_append_memory (new_buffer, mem);
  }

  /* everything needed is copied */
  gst_rtp_buffer_unmap (&rtp);

  /* set ssrc, seqnum and fmtp */
  gst_rtp_buffer_map (new_buffer, GST_MAP_WRITE, &new_rtp);
  gst_rtp_buffer_set_extension (&new_rtp, 1);
  _print_rtp_packet_info(&new_rtp);
  gst_rtp_buffer_unmap (&new_rtp);
  /* Copy over timestamps */

  return new_buffer;
}

static GstFlowReturn
gst_mprtp_sender_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
	GstFlowReturn res;
	GstMprtpSender *mprtps;
	//GstClockTime position, duration;
	mprtps = GST_MPRTP_SENDER (parent);

	GstElement *element = GST_ELEMENT(mprtps);
	//trigger scheduling and calculating
	//buf = gst_mprtp_buffer_new(mprtps, buf);
	buf = _extending_buffer(mprtps, buf);

	/*
	* The _switch function might push a buffer if 'resend-latest' is true.
	*
	* Elements/Applications (e.g. camerabin) might use pad probes to
	* switch output-selector's active pad. If we simply switch and don't
	* recheck any pending pad switch the following codepath could end
	* up pushing a buffer on a non-active pad. This is bad.
	*
	* So we always should check the pending_srcpad before going further down
	* the chain and pushing the new buffer
	*/
	//while (mprtps->subflows) {
	/* Do the switch */
		//gst_mprtp_sender_switch (mprtps);
	//}

	/* Keep track of last stop and use it in SEGMENT start after
	 switching to a new src pad */
//	position = GST_BUFFER_TIMESTAMP (buf);
//	if (GST_CLOCK_TIME_IS_VALID (position)) {
//	duration = GST_BUFFER_DURATION (buf);
//	if (GST_CLOCK_TIME_IS_VALID (duration)) {
//	  position += duration;
//	}
//	GST_LOG_OBJECT (mprtps, "setting last stop %" GST_TIME_FORMAT,
//		GST_TIME_ARGS (position));
//	mprtps->segment.position = position;
//	}
//
//
	GST_LOG_OBJECT (mprtps, "pushing buffer to %" GST_PTR_FORMAT,
	  mprtps->active_srcpad);

	res = gst_pad_push (mprtps->active_srcpad, buf);

	return res;
}

static GstStateChangeReturn
gst_mprtp_sender_change_state (GstElement * element,
    GstStateChange transition)
{
  GstMprtpSender *sel;
  GstStateChangeReturn result;

  sel = GST_MPRTP_SENDER (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }
  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_mprtp_sender_reset (sel);
      break;
    default:
      break;
  }

  return result;
}

static gboolean
gst_mprtp_sender_forward_event (GstMprtpSender* mprtps, GstEvent * event)
{
  gboolean res = TRUE;
  GstPad *active;

  active = gst_mprtp_sender_get_active(mprtps);
  if (active) {
    res = gst_pad_push_event (active, event);
    gst_object_unref (active);
  } else {
    gst_event_unref (event);
  }

  return res;
}

static gboolean
gst_mprtp_sender_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean result = TRUE;
  GstMprtpSender *mprtps;
  //GstPad *active = NULL;

  mprtps = GST_MPRTP_SENDER (parent);

  switch (GST_EVENT_TYPE (event)) {
      case GST_EVENT_EOS:
      {
    	  result = gst_mprtp_sender_forward_event (mprtps, event);
        break;
      }
      case GST_EVENT_SEGMENT:
      {
        gst_event_copy_segment (event, &mprtps->segment);
        GST_DEBUG_OBJECT (mprtps, "configured SEGMENT %" GST_SEGMENT_FORMAT,
            &mprtps->segment);
        /* fall through */
      }
      default:
    	  g_print("----%d----", GST_EVENT_TYPE(event));
    	  //result = gst_pad_event_default (pad, parent, event);
    	  //g_print("%d",result);
    	  result = gst_pad_push_event(mprtps->active_srcpad, event);
    	  g_print("%d\n",result);
        break;
  }

  return result;
}

static gboolean
gst_mprtp_sender_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = TRUE;
//  GstMprtpSender *sel;
//  GstPad *active = NULL;

//  sel = GST_MPRTP_SENDER (parent);

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
	result->congestion = MPRTP_SUBFLOW_NON_CONGESTED;
	return result;
}

void _dispose_subflow(Subflow* target)
{

}

void _subflow_dtor(Subflow* target)
{
	g_free(target);
}


SchNode* _schnode_ctor()
{
	SchNode *result = g_new0(SchNode, 1);
	return result;
}

void _schnode_rdtor(SchNode* target)
{
	if(target == NULL){
		return;
	}
	_schnode_rdtor(target->left);
	_schnode_rdtor(target->right);
	g_free(target);
}

SchNode* _schtree_insert(Subflow* value,
		SchNode *node, guint32* target, guint32 current)
{
	//create the root
	if(node == NULL){
		node = _schnode_ctor();
	}
	if(node->value != NULL){ //The node is a leaf
		return node;
	}
	if(*target >= current && node->left == NULL && node->right == NULL){
		*target-=current;
		node->value = value;
		return node;
	}

	node->left = _schtree_insert(value, node->left, target, current>>1);
	if(*target < 1)
		return node;

	node->right = _schtree_insert(value, node->right, target, current>>1);
		return node;
}

SchNode* _schtree_setnext(SchNode* node)
{
	if(node == NULL){
		return NULL;
	}
	if(node->left == NULL && node->right == NULL){
		return node;
	}
	SchNode *next = node->next;
	node->next = (node->next == node->left) ? node->right : node->left;
	return _schtree_setnext(next);
}

SchNode* _schtree_build(GSList* subflows, Congestion congestion)
{
	Subflow *subflow;
	GSList *it;
	guint32 bw_sum = 0;
	const guint32 tree_max = 128;
	guint32 target;
	SchNode* root = NULL;
	guint32 max_bw = 0;
	if(subflows == NULL){
		return NULL;
	}
	//Aggregating
	for(it = subflows; it != NULL; it = it->next){
		subflow = (Subflow*) it->data;
		if(subflow->congestion != congestion){
			continue;
		}
		if(subflow->bw > max_bw)
			max_bw = subflow->bw;
		if(subflow->bw == 0){ //we have not received any RR from this Subflow
			subflow->bw = max_bw;
		}
		bw_sum += subflow->bw;
	}

	for(it = subflows; it != NULL; it = it->next){
		subflow = (Subflow*) it->data;
		if(subflow->congestion != congestion){
			continue;
		}
		if(bw_sum == 0){ // we have not received any RR report at all
		  target = ((gfloat)tree_max / (gfloat)g_slist_length(subflows));
		}else{
		  target = ((gfloat)subflow->bw / (gfloat)bw_sum) * tree_max;
		}
		root = _schtree_insert(&subflow, root, &target, tree_max);
	}
	return root;
}

void _recalculate_congestion(GstMprtpSender* mprtps)
{
	Subflow *subflow;
	GSList *it;

	//recalculate the categories
	for(it = mprtps->subflows; it != NULL; it = it->next){
		subflow = (Subflow*) it->data;
		subflow->congestion = MPRTP_SUBFLOW_NON_CONGESTED;
	}

	//trigger a scheduling tree
	_refresh_schtree(mprtps);
}

//must be called with LOCK
void _refresh_schtree(GstMprtpSender* mprtps)
{
	if(mprtps->nctree != NULL){
		_schnode_rdtor(mprtps->nctree);
	}
	mprtps->nctree = _schtree_build(mprtps->subflows, MPRTP_SUBFLOW_NON_CONGESTED);
	if(mprtps->ctree != NULL){
		_schnode_rdtor(mprtps->ctree);
	}
	mprtps->ctree = _schtree_build(mprtps->subflows, MPRTP_SUBFLOW_CONGESTED);
	if(mprtps->mctree != NULL){
		_schnode_rdtor(mprtps->mctree);
	}
	mprtps->mctree = _schtree_build(mprtps->subflows, MPRTP_SUBFLOW_MIDLY_CONGESTED);
}
