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
#include <gst/rtp/gstrtcpbuffer.h>
#include "gstmprtpsender.h"
#include "mprtplibs.h"

#define GST_CAT_DEFAULT gst_mprtp_sender_debug

#define gst_mprtp_sender_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstMprtpSender, gst_mprtp_sender,
    GST_TYPE_ELEMENT, GST_DEBUG_CATEGORY_INIT (gst_mprtp_sender_debug, \
            "mprtpsender", 0, "MpRtp Sender"););

#define GST_MPRTCP_BUFFER_FOR_PACKETS(b,buffer,packet) \
  for ((b) = gst_rtcp_buffer_get_first_packet ((buffer), (packet)); (b); \
          (b) = gst_rtcp_packet_move_to_next ((packet)))


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


static GstStaticPadTemplate gst_mprtp_sender_rtcp_sink_factory =
GST_STATIC_PAD_TEMPLATE ("rtcp_sink",
    GST_PAD_SINK,
	GST_PAD_ALWAYS,
    GST_MPRTCP_SR_CAPS);


static GstStaticPadTemplate gst_mprtp_sender_mprtcp_sink_factory =
GST_STATIC_PAD_TEMPLATE ("mprtcp_sink",
    GST_PAD_SINK,
	GST_PAD_ALWAYS,
    GST_MPRTCP_SR_CAPS);


static GstStaticPadTemplate gst_mprtp_sender_mprtcp_src_factory =
GST_STATIC_PAD_TEMPLATE ("mprtcp_src",
    GST_PAD_SRC,
	GST_PAD_ALWAYS,
    GST_MPRTCP_SR_CAPS);


//---------------------------------------------------------------
//-------------------- LOCAL TYPE DEFINITIONS ------------------
//---------------------------------------------------------------

enum
{
  PROP_RTP_EXT_ID,
  PROP_0,
};

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};




struct _SchNode
{
	SchNode *left, *right, *next;
	MPRTPSenderSubflow* value;
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
static GstFlowReturn gst_mprtcp_sender_rtcp_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
static GstFlowReturn gst_mprtcp_sender_mprtcp_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
static GstStateChangeReturn gst_mprtp_sender_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_mprtp_sender_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_mprtp_sender_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_mprtcp_sender_rtcp_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_mprtcp_sender_rtcp_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_mprtcp_sender_mprtcp_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_mprtcp_sender_mprtcp_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static MPRTPSenderSubflow* _select_subflow (GstMprtpSender * mprtps, GstRTPBuffer *rtp);

static void _subflow_dtor(MPRTPSenderSubflow* target);
static void _dispose_subflow(MPRTPSenderSubflow* target);
static MPRTPSenderSubflow* _make_subflow(guint16 id, GstPad* srcpad);
static MPRTPSenderSubflow* _subflow_ctor();

static SchNode* _schnode_ctor();
static void _schnode_rdtor(SchNode* target);
static SchNode* _schtree_insert(MPRTPSenderSubflow* value,
		SchNode *node, guint32* target, guint32 current);
static SchNode* _schtree_setnext(SchNode* root);
static SchNode* _schtree_build(GSList* subflows, MPRTP_Congestion congestion);
static void _refresh_schtree(GstMprtpSender* mprtps);
void _recalculate_congestion(GstMprtpSender* mprtps);
static MPRTPSenderSubflow* _switch_subflow_in_schtree (GstMprtpSender * mprtps, SchNode *category);
static void _print_rtp_packet_info(GstRTPBuffer *rtp);
static GstIterator *
gst_mprtp_sender_iterate_internal_links (GstPad * pad, GstObject * parent);

static void
gst_mprtp_sender_class_init (GstMprtpSenderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->dispose = gst_mprtp_sender_dispose;

  g_object_class_install_property (gobject_class, PROP_RTP_EXT_ID,
	g_param_spec_object ("RTP One byte header extension ID", "RTP ext. ID",
			"The ID used on RTP packet extension for signaling", GST_TYPE_PAD,
			G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
			G_PARAM_STATIC_STRINGS));

  gobject_class->set_property = gst_mprtp_sender_set_property;
  gobject_class->get_property = gst_mprtp_sender_get_property;

  gst_element_class_set_static_metadata (gstelement_class, "MpRtp sender proxy element",
      "Generic", "Multipath Rtp sender proxy element",
      "Balázs Kreith <balazs.kreith@gmail.com>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_mprtp_sender_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&gst_mprtp_sender_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
	gst_static_pad_template_get (&gst_mprtp_sender_rtcp_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
  	gst_static_pad_template_get (&gst_mprtp_sender_mprtcp_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
  	gst_static_pad_template_get (&gst_mprtp_sender_mprtcp_src_factory));

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

  mprtps->rtcp_sinkpad = gst_pad_new_from_static_template (
		  &gst_mprtp_sender_rtcp_sink_factory, "rtcp_sink");
  gst_pad_set_chain_function (mprtps->rtcp_sinkpad,
	  GST_DEBUG_FUNCPTR (gst_mprtcp_sender_rtcp_chain));
  gst_pad_set_event_function (mprtps->rtcp_sinkpad,
  	  GST_DEBUG_FUNCPTR (gst_mprtcp_sender_rtcp_event));
  gst_pad_set_query_function (mprtps->rtcp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_mprtcp_sender_rtcp_query));

  GST_OBJECT_FLAG_SET (mprtps->rtcp_sinkpad, GST_PAD_FLAG_PROXY_CAPS);
  gst_element_add_pad (GST_ELEMENT (mprtps), mprtps->rtcp_sinkpad);

  mprtps->mprtcp_sinkpad = gst_pad_new_from_static_template (
		  &gst_mprtp_sender_rtcp_sink_factory, "mprtcp_sink");
  gst_pad_set_chain_function (mprtps->mprtcp_sinkpad,
	  GST_DEBUG_FUNCPTR (gst_mprtcp_sender_mprtcp_chain));
  gst_pad_set_event_function (mprtps->mprtcp_sinkpad,
  	  GST_DEBUG_FUNCPTR (gst_mprtcp_sender_mprtcp_event));
  gst_pad_set_query_function (mprtps->mprtcp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_mprtcp_sender_mprtcp_query));

  GST_OBJECT_FLAG_SET (mprtps->mprtcp_sinkpad, GST_PAD_FLAG_PROXY_CAPS);
  gst_element_add_pad (GST_ELEMENT (mprtps), mprtps->mprtcp_sinkpad);


  mprtps->mprtcp_srcpad =
  	gst_pad_new_from_static_template (&gst_mprtp_sender_mprtcp_src_factory,
  	"mprtcp_src");
    gst_pad_use_fixed_caps (mprtps->mprtcp_srcpad);
    gst_pad_set_active (mprtps->mprtcp_srcpad, TRUE);
    gst_element_add_pad (GST_ELEMENT_CAST (mprtps), mprtps->mprtcp_srcpad);
  /* srcpad management */
  mprtps->selected_subflow = NULL;
  gst_segment_init (&mprtps->segment, GST_FORMAT_UNDEFINED);
  mprtps->latest_buffer = NULL;
  mprtps->subflows = NULL;


}

static void
gst_mprtp_sender_reset (GstMprtpSender * mprtps)
{
  GSList *it;
  MPRTPSenderSubflow *subflow;
  for(it = mprtps->subflows; it != NULL; it = it->next){
	  subflow = (MPRTPSenderSubflow*) it->data;
	  gst_object_unref(subflow->srcpad);
  }
  g_slist_free(mprtps->subflows);
  gst_object_unref(mprtps->selected_subflow);
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
  guint8* id;
  switch (prop_id) {
  case PROP_RTP_EXT_ID:
	  id = g_value_get_object (value);
	  break;
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

static gboolean
forward_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GstMprtpSender *mprtps = GST_MPRTP_SENDER(user_data);
  MPRTPSenderSubflow *subflow;
  GSList *item;
  gboolean result = TRUE;
  for(item = mprtps->subflows; item != NULL; item = item->next){
	  subflow = (MPRTPSenderSubflow*) item->data;
	  result &= gst_pad_push_event (subflow->srcpad, *event);
  }

  return result;
}


static GstPad* _request_mprtp_srcpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
GstPad* _request_mprtp_srcpad(GstElement * element,
		GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
	GstPad *srcpad;
	GstMprtpSender *mprtps;
	MPRTPSenderSubflow* subflow;
	guint16 subflow_id;

	mprtps = GST_MPRTP_SENDER (element);
    g_print("\n\nNEW MPRTP SRC PAD REQUESTED\n\n");
	GST_DEBUG_OBJECT (mprtps, "requesting pad");
	GST_OBJECT_LOCK (mprtps);

	sscanf(name, "mprtp_src_%hu", &subflow_id);
	srcpad = gst_pad_new_from_template (templ, name);
	subflow = _make_subflow(subflow_id, srcpad);
	mprtps->subflows = g_slist_append(mprtps->subflows, subflow);
	gst_object_ref(subflow->srcpad);
	GST_OBJECT_UNLOCK (mprtps);
	gst_pad_set_active (srcpad, TRUE);

	/* Forward sticky events to the new srcpad */
	gst_pad_sticky_events_foreach (mprtps->sinkpad, forward_sticky_events, mprtps);

	gst_element_add_pad (GST_ELEMENT (mprtps), srcpad);

	if(mprtps->selected_subflow == NULL){
		mprtps->selected_subflow = subflow;
	}
	return srcpad;
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

SchNode* _select_category(GstMprtpSender *mprtps, GstRTPBuffer *rtp)
{
	SchNode* selected = NULL;
	if(mprtps->nctree == mprtps->mctree && mprtps->mctree == NULL){
		_recalculate_congestion(mprtps);
	}
	//the category choose algorithm comes here
	selected = mprtps->nctree;
	return selected;
}

//must be called with lock on mprtps and rtp must be mapped
static MPRTPSenderSubflow*
_select_subflow_for_special_cases(GstMprtpSender * mprtps, GstRTPBuffer *rtp)
{
	MPRTPSenderSubflow *result = NULL;
	//implement special case subflow calls here.
	return result;
}

static void _send_segment_event(GstMprtpSender *mprtps, GstPad *outpad)
{
	GstSegment *seg;
	GstEvent *ev;
	gst_pad_sticky_events_foreach (mprtps->sinkpad, forward_sticky_events, outpad);

	/* update segment if required */
	if (mprtps->segment.format == GST_FORMAT_UNDEFINED) {
		return;
	}
	/* Send SEGMENT to the pad we are going to switch to */
	seg = &mprtps->segment;

	ev = gst_event_new_segment (seg);

	if (!gst_pad_push_event (outpad, ev)) {
	GST_WARNING_OBJECT (mprtps,
		"new segment handling failed in %" GST_PTR_FORMAT,
		outpad);
	}
}

//must be called with lock on mprtps and rtp must be mapped
MPRTPSenderSubflow* _select_subflow(GstMprtpSender * mprtps, GstRTPBuffer *rtp)
{
	MPRTPSenderSubflow *result = mprtps->selected_subflow;
	SchNode *category = NULL;
	//we need to decide the category based on the rtp
	//need to decide the category here.
	category = _select_category(mprtps, rtp);
	if(category == NULL){
		GST_WARNING_OBJECT(mprtps, "No available category for subflows!");
		return NULL;
	}
	if(!gst_rtp_buffer_get_marker(rtp)){
		return result;
	}
	result = _switch_subflow_in_schtree(mprtps, category);
	//resend segment
	_send_segment_event(mprtps, result->srcpad);
	return result;
}
//must be called with object lock, the category must not be null.
MPRTPSenderSubflow* _switch_subflow_in_schtree (GstMprtpSender * mprtps, SchNode *category)
{
  SchNode *node = NULL;
  MPRTPSenderSubflow *result = NULL;
  gint8 count;

  /* Switch */
  GST_LOG_OBJECT (mprtps, "Select the next subflow in scheuling tree");

  //since there is a chance that the sum of the node target values is not equal to the max_tree
  for(count = 0; node == NULL && count < 2; ++count){
	  node = _schtree_setnext(category);
  }
  if(G_UNLIKELY(node == NULL)){
	  GST_LOG_OBJECT (mprtps, "Not available subflow in the selected tree");
  }
  result = node->value;
  return result;

}

static void _fillup_subflow_infos(MpRtpHeaderExtSubflowInfos* subflow_info, guint16 subflow_id, guint16 seq_num)
{
	subflow_info->subflow_id = subflow_id;
	subflow_info->subflow_seq_num = seq_num;
}
//must be called with G_OBJECT_LOCK
static void _extending_buffer(GstRTPBuffer *rtp, MPRTPSenderSubflow* selected);
void _extending_buffer(GstRTPBuffer *rtp, MPRTPSenderSubflow* selected)
{
	MpRtpHeaderExtSubflowInfos data;
	_fillup_subflow_infos(&data, selected->id, ++selected->sent);
	gst_rtp_buffer_add_extension_onebyte_header(rtp, MPRTP_EXT_HEADER_ID, (gpointer) &data, sizeof(data));
}


static GstFlowReturn
gst_mprtp_sender_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
	GstMprtpSender *mprtps;
	GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
	GstBuffer *outbuf;
	GstPad *outpad;

	mprtps = GST_MPRTP_SENDER (parent);
	GST_OBJECT_LOCK (GST_OBJECT (mprtps));

	outbuf = gst_buffer_make_writable (buf);
	if (G_UNLIKELY (!gst_rtp_buffer_map(outbuf, GST_MAP_READWRITE, &rtp))){
		GST_WARNING_OBJECT(mprtps, "The RTP packet is not read and writeable");
		GST_OBJECT_UNLOCK(GST_OBJECT (mprtps));
		return GST_FLOW_ERROR;
	}
	//implement special case chooses here
	//mprtps->selected_subflow = _select_subflow_for_special_cases(mprtps, rtp);
	mprtps->selected_subflow = _select_subflow(mprtps, &rtp);
	if(mprtps->selected_subflow == NULL){
		GST_WARNING_OBJECT(mprtps, "The selected subflow is NULL");
		gst_rtp_buffer_unmap(&rtp);
		GST_OBJECT_UNLOCK(GST_OBJECT (mprtps));
		return GST_FLOW_ERROR;
	}

	_extending_buffer(&rtp, mprtps->selected_subflow);
	outpad = mprtps->selected_subflow->srcpad;
	_print_rtp_packet_info(&rtp);
	gst_rtp_buffer_unmap(&rtp);

	if (!gst_pad_is_linked (outpad)) {
	  GST_ERROR_OBJECT(mprtps, "The selected pad (%"GST_PTR_FORMAT") is not linked", outpad);
	  GST_OBJECT_UNLOCK(GST_OBJECT (mprtps));
	  return GST_FLOW_ERROR;
	}

	mprtps->latest_buffer = outbuf;
	GST_OBJECT_UNLOCK(GST_OBJECT (mprtps));

	GST_LOG_OBJECT (mprtps, "pushing buffer to subflow %d at %" GST_PTR_FORMAT" pad",
	  mprtps->selected_subflow->id, outpad);

	return gst_pad_push (outpad, outbuf);
}


static GstFlowReturn
gst_mprtcp_sender_rtcp_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
	GstMprtpSender *mprtps;
	GstRTCPBuffer rtcp = {NULL, };
	gboolean more;
	GstRTCPPacket packet;

	if(!gst_rtcp_buffer_map (buf, GST_MAP_READ, &rtcp)){
		GST_WARNING("The RTCP buffer is not readable");
		return GST_FLOW_ERROR;
	}

	mprtps = GST_MPRTP_SENDER (parent);
	GST_OBJECT_LOCK (GST_OBJECT (mprtps));

	GST_MPRTCP_BUFFER_FOR_PACKETS (more, &rtcp, &packet); {
	/* first packet must be SR or RR or else the validate would have failed */
	switch (gst_rtcp_packet_get_type (&packet)) {
	  case GST_RTCP_TYPE_SR:
		//we extend the sender reports by adding the subflow informations.
		GST_LOG_OBJECT(mprtps, "RTP Sender report is received and going to be nested into an MPRTCP packet.");
		break;
	  default:
		/* we can ignore these packets */
		break;
	}
	}
	gst_rtcp_buffer_unmap (&rtcp);
	GST_OBJECT_UNLOCK(GST_OBJECT (mprtps));

	if (!gst_pad_is_linked (mprtps->mprtcp_srcpad)) {
	  GST_ERROR_OBJECT(mprtps, "The MPRTCP sink is connected, but src is not");
	  return GST_FLOW_ERROR;
	}

	GST_LOG_OBJECT (mprtps, "pushing mprtcp buffer to mprtcp srcpad");
	return gst_pad_push (mprtps->mprtcp_srcpad, buf);
}



static GstFlowReturn
gst_mprtcp_sender_mprtcp_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
	GstMprtpSender *mprtps;
	GstRTCPBuffer rtcp = {NULL, };
	gboolean more;
	GstRTCPPacket packet;

	if(!gst_rtcp_buffer_map (buf, GST_MAP_READ, &rtcp)){
		GST_WARNING("The RTCP buffer is not readable");
		return GST_FLOW_ERROR;
	}

	mprtps = GST_MPRTP_SENDER (parent);
	GST_OBJECT_LOCK (GST_OBJECT (mprtps));

	GST_MPRTCP_BUFFER_FOR_PACKETS (more, &rtcp, &packet); {
	/* first packet must be SR or RR or else the validate would have failed */
	switch (gst_rtcp_packet_get_type (&packet)) {
	  case MPRTCP_PACKET_TYPE_IDENTIFIER:
		//we extend the sender reports by adding the subflow informations.
		GST_LOG_OBJECT(mprtps, "MPRTCP packet is going to be processed at MPRTP Sender proxy.");
		break;
	  default:
		/* we can ignore these packets */
		break;
	}
	}

	return GST_FLOW_OK;
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
  gboolean result = TRUE;
  GstPad *pad;
  MPRTPSenderSubflow *subflow;
  GSList *item;
  g_print("gst_mprtp_sender_forward_event\n");
  for(item = mprtps->subflows; item != NULL; item = item->next){
	  subflow = (MPRTPSenderSubflow*) item->data;
	  pad = subflow->srcpad;
	  g_print("Pushing event to srcpad %d\n", subflow->id);
	  if(pad == NULL){
		  continue;
	  }

	  result &= gst_pad_push_event (pad, event);
  }
  return result;
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
    	  //result = gst_pad_push_event(mprtps->selected_subflow->srcpad, event);
    	  result = gst_mprtp_sender_forward_event (mprtps, event);
    	  g_print("%d\n",result);
        break;
  }

  return result;
}

static gboolean
gst_mprtp_sender_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = TRUE;
  GstMprtpSender *mprtps;
  GstPad *srcpad = NULL;
  MPRTPSenderSubflow *subflow;
  GSList *item;

  mprtps = GST_MPRTP_SENDER (parent);

  switch (GST_QUERY_TYPE (query)) {
  	case GST_QUERY_CAPS:
    default:
	for(item = mprtps->subflows; item != NULL; item = item->next){
		  subflow = (MPRTPSenderSubflow*) item->data;
		  srcpad = subflow->srcpad;
		  if(srcpad == NULL){
			  continue;
		  }
		  res &= gst_pad_peer_query (srcpad, query);
		  //g_print("QUERYsrcpad->padtemplate->name_template
	  }
      //res = gst_pad_proxy_query_caps (pad, query);
      //res = gst_pad_query_default (pad, parent, query);
      break;
  }

  return res;
}


gboolean
gst_mprtcp_sender_rtcp_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMprtpSender *mprtps;

  mprtps = GST_MPRTP_SENDER (parent);

  return gst_pad_push_event (mprtps->mprtcp_srcpad, event);
}

gboolean
gst_mprtcp_sender_rtcp_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
	GstMprtpSender *mprtps;

	mprtps = GST_MPRTP_SENDER (parent);

	return gst_pad_peer_query (mprtps->mprtcp_srcpad, query);
}


gboolean
gst_mprtcp_sender_mprtcp_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  return FALSE;
}

gboolean
gst_mprtcp_sender_mprtcp_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
   return FALSE;
}



GstIterator *
gst_mprtp_sender_iterate_internal_links (GstPad * pad, GstObject * parent)
{
  GstMprtpSender *mprtps;
  GstPad *otherpad = NULL;
  GstIterator *it = NULL;

  mprtps = GST_MPRTP_SENDER (parent);

  GST_OBJECT_LOCK(GST_OBJECT (mprtps));
  if (pad == mprtps->mprtcp_sinkpad) {
    otherpad = gst_object_ref (mprtps->mprtcp_sinkpad);
  } else if (pad == mprtps->rtcp_sinkpad) {
    otherpad = gst_object_ref (mprtps->rtcp_sinkpad);
  } else if (pad == mprtps->mprtcp_srcpad) {
	otherpad = gst_object_ref (mprtps->mprtcp_srcpad);
  }
  GST_OBJECT_UNLOCK(GST_OBJECT (mprtps));

  if (otherpad) {
    GValue val = { 0, };

    g_value_init (&val, GST_TYPE_PAD);
    g_value_set_object (&val, otherpad);
    it = gst_iterator_new_single (GST_TYPE_PAD, &val);
    g_value_unset (&val);
    gst_object_unref (otherpad);
  } else {
    it = gst_iterator_new_single (GST_TYPE_PAD, NULL);
  }

  return it;
}


//---------------------------------------------------------------
//-------- PRIVATE FUNCTIONS DEFINITIONS ------------------------
//---------------------------------------------------------------

MPRTPSenderSubflow* _subflow_ctor()
{
	MPRTPSenderSubflow* result;
	result = (MPRTPSenderSubflow*) g_malloc0(sizeof(MPRTPSenderSubflow));
	return result;
}

MPRTPSenderSubflow* _make_subflow(guint16 id, GstPad* srcpad)
{
	MPRTPSenderSubflow* result = _subflow_ctor();
	result->id = id;
	result->srcpad = srcpad;
	result->congestion = MPRTP_SUBFLOW_NON_CONGESTED;
	result->initiated = FALSE;
	return result;
}

void _dispose_subflow(MPRTPSenderSubflow* target)
{

}

void _subflow_dtor(MPRTPSenderSubflow* target)
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

SchNode* _schtree_insert(MPRTPSenderSubflow* value,
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

SchNode* _schtree_build(GSList* subflows, MPRTP_Congestion congestion)
{
	MPRTPSenderSubflow *subflow;
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
		subflow = (MPRTPSenderSubflow*) it->data;
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
		subflow = (MPRTPSenderSubflow*) it->data;
		if(subflow->congestion != congestion){
			continue;
		}
		if(bw_sum == 0){ // we have not received any RR report at all
		  target = ((gfloat)tree_max / (gfloat)g_slist_length(subflows));
		}else{
		  target = ((gfloat)subflow->bw / (gfloat)bw_sum) * tree_max;
		}

		root = _schtree_insert(subflow, root, &target, tree_max);
	}
	return root;
}

void _recalculate_congestion(GstMprtpSender* mprtps)
{
	MPRTPSenderSubflow *subflow;
	GSList *it;

	//recalculate the categories
	for(it = mprtps->subflows; it != NULL; it = it->next){
		subflow = (MPRTPSenderSubflow*) it->data;
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



void _print_rtp_packet_info(GstRTPBuffer *rtp)
{
	gboolean extended;
	g_print(
   "0               1               2               3          \n"
   "0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 \n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%3d|%1d|%1d|%7d|%1d|%13d|%31d|\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%63u|\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%63u|\n",
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
		guint16 bits;
		guint8 *pdata;
		guint wordlen;
		gulong index = 0;

		gst_rtp_buffer_get_extension_data (rtp, &bits, (gpointer) & pdata, &wordlen);


		g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|0x%-29X|%31d|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n",
	   bits,
	   wordlen);

	   for(index = 0; index < wordlen; ++index){
		 g_print("|0x%-5X = %5d|0x%-5X = %5d|0x%-5X = %5d|0x%-5X = %5d|\n"
				 "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n",
				 *(pdata+index*4), *(pdata+index*4),
				 *(pdata+index*4+1),*(pdata+index*4+1),
				 *(pdata+index*4+2),*(pdata+index*4+2),
				 *(pdata+index*4+3),*(pdata+index*4+3));
	  }
	  g_print("+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n");
	}
}
