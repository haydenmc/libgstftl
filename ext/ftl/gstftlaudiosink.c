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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstftlaudiosink.h"

#include "gstftlsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_ftl_audio_sink_debug_category);
#define GST_CAT_DEFAULT gst_ftl_audio_sink_debug_category

struct _GstFtlAudioSink
{
  GstBaseSink parent_instance;
};

static GstFlowReturn gst_ftl_audio_sink_render (GstBaseSink * sink,
    GstBuffer * buffer);

/* pad templates */

static GstStaticPadTemplate gst_ftl_audio_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_FTL_AUDIO_SINK_CAPS)
    );

/* class initialization */

G_DEFINE_TYPE (GstFtlAudioSink, gst_ftl_audio_sink, GST_TYPE_BASE_SINK);

static void
gst_ftl_audio_sink_class_init (GstFtlAudioSinkClass * klass)
{
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_ftl_audio_sink_debug_category, "ftlaudiosink", 0,
      "debug category for ftlaudiosink element");

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "FTL audio sink", "Sink", "Internal audio sink of ftlsink",
      "Make.TV, Inc. <info@make.tv>");

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_ftl_audio_sink_template);

  base_sink_class->render = GST_DEBUG_FUNCPTR (gst_ftl_audio_sink_render);
}

static void
gst_ftl_audio_sink_init (GstFtlAudioSink * self)
{
}

static GstFlowReturn
gst_ftl_audio_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstFtlAudioSink *self = GST_FTL_AUDIO_SINK (sink);
  GstFtlSink *parent = GST_FTL_SINK (GST_OBJECT_PARENT (self));
  GstClockTime time;
  GstMapInfo map;
  gint bytes_sent;

  if (!gst_ftl_sink_connect (parent)) {
    return GST_FLOW_ERROR;
  }

  time = GST_BUFFER_DTS_OR_PTS (buffer);
  if (!GST_CLOCK_TIME_IS_VALID (time)) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED, ("Got buffer without timestamp"),
        ("%" GST_PTR_FORMAT, buffer));
    return GST_FLOW_ERROR;
  }

  time = gst_segment_to_running_time (&sink->segment, GST_FORMAT_TIME, time);

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map %" GST_PTR_FORMAT, buffer);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "sending %" G_GSIZE_FORMAT " bytes at %"
      GST_TIME_FORMAT, map.size, GST_TIME_ARGS (time));

  bytes_sent = ftl_ingest_send_media_dts (gst_ftl_sink_get_handle (parent),
      FTL_AUDIO_DATA, GST_TIME_AS_USECONDS (time), map.data, map.size, 1);

  gst_buffer_unmap (buffer, &map);

  GST_LOG_OBJECT (self, "sent %d bytes", bytes_sent);
  return GST_FLOW_OK;
}
