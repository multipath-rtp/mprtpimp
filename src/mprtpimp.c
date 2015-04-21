/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst.h>
#include "mprtplibs.h"
#include "gstmprtpreceiver.h"
#include "gstmprtpsender.h"


static gboolean
mprtpimp_init (GstPlugin * plugin)
{
	GST_DEBUG_CATEGORY_INIT (gst_mprtp_receiver_debug, "mprtpreceiver",
			  0, "MpRtpReceiver");

	GST_DEBUG_CATEGORY_INIT (gst_mprtp_sender_debug, "mprtpsender",
			  0, "MpRtpSender");

  if (!gst_element_register (plugin, "mprtpsender", GST_RANK_NONE,
		  GST_TYPE_MPRTP_SENDER))
    return FALSE;

  if (!gst_element_register (plugin, "mprtpreceiver", GST_RANK_NONE,
          GST_TYPE_MPRTP_RECEIVER))
    return FALSE;

  return TRUE;
}



/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "mprtpimp"
#endif

/* gstreamer looks for this structure to register mprtpsenders
 *
 * exchange the string 'Template mprtpsender' with your mprtpsender description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    mprtpimp,
    "Mprtp protocol implementation",
	mprtpimp_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
