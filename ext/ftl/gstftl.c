/*
 * GStreamer
 * Copyright (C) 2019 Make.TV, Inc. <info@make.tv>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * SECTION:ftl-plugin
 *
 * Sink for Microsoft Mixer's FTL protocol
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 videotestsrc ! timeoverlay !
video/x-raw,width=1920,height=1080,framerate=30/1 ! x264enc bframes=0 b-adapt=0
key-int-max=30 speed-preset=superfast tune=zerolatency bitrate=2800 ! tee name=t
t. ! queue ! h264parse ! avdec_h264 ! videoconvert ! xvimagesink t. ! queue !
f.videosink audiotestsrc ! opusenc ! f.audiosink ftlsink name=f
stream-key=<your-mixer-key>
 * ]|
 *
 * This pipeline sends audio and video to Microsoft Mixer.  It also
 * renders video in your local host, so you can visualy compare
 * the low latency of FTL.
 *
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstftlsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_debug_ftl);
#define GST_CAT_DEFAULT gst_debug_ftl

static gboolean
plugin_init (GstPlugin * plugin)
{
  ftl_status_t status_code;

  GST_DEBUG_CATEGORY_INIT (gst_debug_ftl, "ftl", 0,
      "debug category for ftl plugin");

  status_code = ftl_init ();
  if (status_code != FTL_SUCCESS) {
    GST_ERROR_OBJECT (plugin, "Failed to initialize FTL library: %s",
        ftl_status_code_to_string (status_code));
    return FALSE;
  }

  if (!gst_element_register (plugin, "ftlsink",
          GST_RANK_NONE, GST_TYPE_FTL_SINK)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, ftl,
    "Plugin to send audio and video using Microsoft Mixer's FTL protocol",
    plugin_init, VERSION, "MIT/X11", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
