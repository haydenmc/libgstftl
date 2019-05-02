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

#include "gstftlenums.h"

GstDebugLevel
gst_ftl_log_severity_to_level (ftl_log_severity_t value)
{
  switch (value) {
    case FTL_LOG_CRITICAL:
      return GST_LEVEL_ERROR;
    case FTL_LOG_ERROR:
      return GST_LEVEL_ERROR;
    case FTL_LOG_WARN:
      return GST_LEVEL_WARNING;
    case FTL_LOG_INFO:
      return GST_LEVEL_INFO;
    case FTL_LOG_DEBUG:
      return GST_LEVEL_DEBUG;
    default:
      return GST_LEVEL_LOG;
  }
}

const gchar *
gst_ftl_status_type_get_nick (ftl_status_types_t value)
{
  switch (value) {
    case FTL_STATUS_NONE:
      return "none";
    case FTL_STATUS_LOG:
      return "log";
    case FTL_STATUS_EVENT:
      return "event";
    case FTL_STATUS_VIDEO_PACKETS:
      return "video-packets";
    case FTL_STATUS_VIDEO_PACKETS_INSTANT:
      return "video-packets-instant";
    case FTL_STATUS_AUDIO_PACKETS:
      return "audio-packets";
    case FTL_STATUS_VIDEO:
      return "video";
    case FTL_STATUS_AUDIO:
      return "audio";
    case FTL_STATUS_FRAMES_DROPPED:
      return "frames-dropped";
    case FTL_STATUS_NETWORK:
      return "network";
    case FTL_BITRATE_CHANGED:
      return "bitrate-changed";
    default:
      return "<unknown>";
  }
}

const gchar *
gst_ftl_status_event_type_get_nick (ftl_status_event_types_t value)
{
  switch (value) {
    case FTL_STATUS_EVENT_TYPE_UNKNOWN:
      return "unknown";
    case FTL_STATUS_EVENT_TYPE_CONNECTED:
      return "connected";
    case FTL_STATUS_EVENT_TYPE_DISCONNECTED:
      return "disconnected";
    case FTL_STATUS_EVENT_TYPE_DESTROYED:
      return "destroyed";
    case FTL_STATUS_EVENT_INGEST_ERROR_CODE:
      return "ingest-error-code";
    default:
      return "<unknown>";
  }
}

const gchar *
gst_ftl_status_event_reason_get_nick (ftl_status_event_reasons_t value)
{
  switch (value) {
    case FTL_STATUS_EVENT_REASON_NONE:
      return "none";
    case FTL_STATUS_EVENT_REASON_NO_MEDIA:
      return "no-media";
    case FTL_STATUS_EVENT_REASON_API_REQUEST:
      return "api-request";
    case FTL_STATUS_EVENT_REASON_UNKNOWN:
      return "unknown";
    default:
      return "<unknown>";
  }
}

const gchar *
gst_ftl_bitrate_changed_type_get_nick (ftl_bitrate_changed_type_t value)
{
  switch (value) {
    case FTL_BITRATE_DECREASED:
      return "decreased";
    case FTL_BITRATE_INCREASED:
      return "increased";
    case FTL_BITRATE_STABILIZED:
      return "stabilized";
    default:
      return "<unknown>";
  }
}

const gchar *
gst_ftl_bitrate_changed_reason_get_nick (ftl_bitrate_changed_reason_t value)
{
  switch (value) {
    case FTL_BANDWIDTH_CONSTRAINED:
      return "bandwidth-constrained";
    case FTL_UPGRADE_EXCESSIVE:
      return "upgrade-excessive";
    case FTL_BANDWIDTH_AVAILABLE:
      return "bandwidth-available";
    case FTL_STABILIZE_ON_LOWER_BITRATE:
      return "stabilize-on-lower-bitrate";
    case FTL_STABILIZE_ON_ORIGINAL_BITRATE:
      return "stabilize-on-original-bitrate";
    default:
      return "<unknown>";
  }
}
