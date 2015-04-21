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



//#define GST_MPRTP_PAD_CAPS "application/x-rtp"
#define GST_MPRTP_PAD_CAPS GST_STATIC_CAPS_ANY
//#define GST_MPRTCP_RR_CAPS "application/x-rtp"
#define GST_MPRTCP_RR_CAPS GST_STATIC_CAPS_ANY
//#define GST_MPRTCP_SR_CAPS "application/x-rtp"
#define GST_MPRTCP_SR_CAPS GST_STATIC_CAPS_ANY

#endif /* __SRC_MPRTPLIBS_H__ */
