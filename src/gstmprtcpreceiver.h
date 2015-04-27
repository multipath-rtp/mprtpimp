/* GStreamer
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
 
#ifndef __GST_MPRTCP_RR_DEMUX_SENDER_H__
#define __GST_MPRTCP_RR_DEMUX_SENDER_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include "mprtplibs.h"
G_BEGIN_DECLS

#define GST_TYPE_MPRTCP_RECEIVER \
  (gst_mprtcp_receiver_get_type())
#define GST_MPRTCP_RECEIVER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MPRTCP_RECEIVER, GstMpRtcpReceiver))
#define GST_MPRTCP_RECEIVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MPRTCP_RECEIVER, GstMpRtcpReceiverClass))
#define GST_IS_MPRTCP_RECEIVER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MPRTCP_RECEIVER))
#define GST_IS_MPRTCP_RECEIVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MPRTCP_RECEIVER))

typedef struct _GstMpRtcpReceiver GstMpRtcpReceiver;
typedef struct _GstMpRtcpReceiverClass GstMpRtcpReceiverClass;

struct _GstMpRtcpReceiver {
  GstElement element;

  GstPad *sinkpad;
  GstPad *mprtcp_srcpad;
  GstPad *rtcp_srcpad;

};

struct _GstMpRtcpReceiverClass {
  GstElementClass parent_class;
};



G_GNUC_INTERNAL GType gst_mprtcp_receiver_get_type (void);

G_END_DECLS

#endif /* __GST_MPRTCP_RR_DEMUX_SENDER_H__ */
