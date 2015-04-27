/*
 * mprtpsubflow.h
 *
 *  Created on: Apr 16, 2015
 *      Author: balazs
 */

#ifndef __SRC_MPRTPLIBS_H__
#define __SRC_MPRTPLIBS_H__
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_mprtp_receiver_debug);
GST_DEBUG_CATEGORY_STATIC (gst_mprtp_sender_debug);
GST_DEBUG_CATEGORY_STATIC (gst_mprtcp_receiver_debug);

//defines the RTP header extension one byte local ID
#define MPRTP_EXT_HEADER_ID 3
#define MPRTCP_PACKET_TYPE_IDENTIFIER 211

//#define GST_MPRTP_PAD_CAPS "application/x-rtp"
#define GST_MPRTP_PAD_CAPS GST_STATIC_CAPS_ANY
//#define GST_MPRTCP_RR_CAPS "application/x-rtp"
#define GST_MPRTCP_RR_CAPS GST_STATIC_CAPS_ANY
//#define GST_MPRTCP_SR_CAPS "application/x-rtp"
#define GST_MPRTCP_SR_CAPS GST_STATIC_CAPS_ANY

typedef struct _MpRtpHeaderExtSubflowInfos MpRtpHeaderExtSubflowInfos;


typedef enum
{
  MPRTP_SUBFLOW_CONGESTED       = 1,
  MPRTP_SUBFLOW_NON_CONGESTED   = 2,
  MPRTP_SUBFLOW_MIDLY_CONGESTED = 3
}MPRTP_Congestion;


struct _MpRtpHeaderExtSubflowInfos{
	guint16 subflow_id;
	guint16 subflow_seq_num;
};

#endif /* __SRC_MPRTPLIBS_H__ */
