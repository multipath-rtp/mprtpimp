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
 
#ifndef __GST_MPRTP_RECEIVER_H__
#define __GST_MPRTP_RECEIVER_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>

G_BEGIN_DECLS

#define GST_TYPE_MPRTP_RECEIVER \
  (gst_mprtp_receiver_get_type())
#define GST_MPRTP_RECEIVER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MPRTP_RECEIVER, GstMprtpReceiver))
#define GST_MPRTP_RECEIVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MPRTP_RECEIVER, GstMprtpReceiverClass))
#define GST_IS_MPRTP_RECEIVER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MPRTP_RECEIVER))
#define GST_IS_MPRTP_RECEIVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MPRTP_RECEIVER))

typedef struct _GstMprtpReceiver GstMprtpReceiver;
typedef struct _GstMprtpReceiverClass GstMprtpReceiverClass;

struct _GstMprtpReceiver {
  GstElement element;

  GstPad *srcpad;
  GHashTable *subflows;

  GstRTPBuffer RTPBuffer;

};

struct _GstMprtpReceiverClass {
  GstElementClass parent_class;
};


G_GNUC_INTERNAL GType gst_mprtp_receiver_get_type (void);

G_END_DECLS

#endif /* __GST_MPRTP_RECEIVER_H__ */
