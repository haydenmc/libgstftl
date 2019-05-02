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

#include "gstftlvideosink.h"

#include "gstftlsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_ftl_video_sink_debug_category);
#define GST_CAT_DEFAULT gst_ftl_video_sink_debug_category

/* class header */

struct _GstFtlVideoSink
{
  GstBaseSink parent_instance;
};

/* prototypes */

static GstFlowReturn gst_ftl_video_sink_render (GstBaseSink * sink,
    GstBuffer * buffer);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_ftl_video_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_FTL_VIDEO_SINK_CAPS)
    );

/* class initialization */

G_DEFINE_TYPE (GstFtlVideoSink, gst_ftl_video_sink, GST_TYPE_BASE_SINK);

static void
gst_ftl_video_sink_class_init (GstFtlVideoSinkClass * klass)
{
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_ftl_video_sink_debug_category, "ftlvideosink", 0,
      "debug category for ftlvideosink element");

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "FTL video sink", "Sink", "Internal video sink of ftlsink",
      "Make.TV, Inc. <info@make.tv>");

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_ftl_video_sink_template);

  base_sink_class->render = GST_DEBUG_FUNCPTR (gst_ftl_video_sink_render);
}

static void
gst_ftl_video_sink_init (GstFtlVideoSink * self)
{
}

static guint8 *
get_next_nalu (guint8 * input, gsize len, gsize * last_len)
{
  guint32 start_code = 0xFFFFFFFF;

  for (gsize pos = 0; pos < len; pos++) {
    start_code = (start_code << 8) | input[pos];

    if ((start_code & 0xFFFFFF) == 1) {
      if (last_len)
        *last_len = pos - (start_code == 1 ? 3 : 2);
      return input + pos + 1;
    }
  }

  if (last_len)
    *last_len = len;
  return NULL;
}

static GstFlowReturn
gst_ftl_video_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstFtlVideoSink *self = GST_FTL_VIDEO_SINK (sink);
  GstFtlSink *parent = (GstFtlSink *) GST_OBJECT_PARENT (self);
  GstClockTime time;
  GstMapInfo map;
  gint bytes_sent = 0;
  guint num_nalus = 0;
  guint8 *data, *end;

  if (!gst_ftl_sink_connect (parent)) {
    return GST_FLOW_ERROR;
  }

  time = GST_BUFFER_DTS (buffer);
  if (!GST_CLOCK_TIME_IS_VALID (time)) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED, ("Got buffer without DTS"),
        ("%" GST_PTR_FORMAT, buffer));
    return GST_FLOW_ERROR;
  }

  time = gst_segment_to_running_time (&sink->segment, GST_FORMAT_TIME, time);

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED, ("Failed to map buffer"),
        ("%" GST_PTR_FORMAT, buffer));
    return GST_FLOW_ERROR;
  }

  data = get_next_nalu (map.data, map.size, NULL);
  end = map.data + map.size;

  while (data != NULL) {
    gsize nalu_len;
    guint8 *next = get_next_nalu (data, end - data, &nalu_len);
    guint8 nalu_type = data[0] & 0x1f;

    num_nalus++;

    switch (nalu_type) {
      case 0:
        GST_ELEMENT_ERROR (self, STREAM, DECODE, ("Invalid NALU type 0"),
            ("%" GST_PTR_FORMAT, buffer));
        gst_buffer_unmap (buffer, &map);
        return GST_FLOW_ERROR;
      case 9:                  /* AU delimiter */
        GST_LOG_OBJECT (self, "skipping AU delimiter (size %" G_GSIZE_FORMAT
            ")", nalu_len);
        break;
      default:
      {
        gboolean last = (next == NULL);
        gint sent = ftl_ingest_send_media_dts (gst_ftl_sink_get_handle (parent),
            FTL_VIDEO_DATA, gst_util_uint64_scale_round (time, 1, GST_USECOND),
            data, nalu_len, last);

        GST_LOG_OBJECT (self,
            "sent %d bytes (NALU type %u, size %" G_GSIZE_FORMAT "%s) at %"
            GST_TIME_FORMAT, sent, nalu_type, nalu_len, (last ? ", last" : ""),
            GST_TIME_ARGS (time));

        bytes_sent += sent;
        break;
      }
    }

    data = next;
  }

  gst_buffer_unmap (buffer, &map);

  if (num_nalus == 0) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE, ("No NALU in buffer"),
        ("%" GST_PTR_FORMAT, buffer));
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "sent %u NALUs, %d bytes for %" GST_PTR_FORMAT,
      num_nalus, bytes_sent, buffer);
  return GST_FLOW_OK;
}
