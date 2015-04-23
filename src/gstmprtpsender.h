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
 
#ifndef __GST_MPRTP_SENDER_H__
#define __GST_MPRTP_SENDER_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include "mprtplibs.h"
G_BEGIN_DECLS

#define GST_TYPE_MPRTP_SENDER \
  (gst_mprtp_sender_get_type())
#define GST_MPRTP_SENDER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MPRTP_SENDER, GstMprtpSender))
#define GST_MPRTP_SENDER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MPRTP_SENDER, GstMprtpSenderClass))
#define GST_IS_MPRTP_SENDER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MPRTP_SENDER))
#define GST_IS_MPRTP_SENDER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MPRTP_SENDER))

typedef struct _GstMprtpSender GstMprtpSender;
typedef struct _GstMprtpSenderClass GstMprtpSenderClass;
typedef struct _SchNode SchNode;
typedef struct _MPRTPSenderSubflow MPRTPSenderSubflow;

struct _GstMprtpSender {
  GstElement element;

  GstPad *sinkpad;
  GSList *subflows;
  MPRTPSenderSubflow *selected_subflow;
  GstSegment segment;

  //Necessary for packet scheduling
  SchNode *ctree;
  SchNode *mctree;
  SchNode *nctree;

  //These values are intended to use in different methods and the
  //purpuse of declared here is to reserve the memory only once.
  GstRTPBuffer RTPBuffer;

};

struct _GstMprtpSenderClass {
  GstElementClass parent_class;
};



struct _MPRTPSenderSubflow{
	guint16 id;
    guint16 seq_num;
    guint16 sent;
    GstPad* srcpad;
    guint32 bw;
    MPRTP_Congestion congestion;
};

G_GNUC_INTERNAL GType gst_mprtp_sender_get_type (void);

G_END_DECLS

#endif /* __GST_MPRTP_SENDER_H__ */
