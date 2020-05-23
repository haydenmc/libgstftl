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
 * SECTION:element-gstftlsink
 *
 * The ftlsink element is a wraper of FTL.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 videotestsrc ! video/x-raw,width=640,height=360,framerate=30/1 ! x264enc bframes=0 b-adapt=0 key-int-max=30 speed-preset=superfast tune=zerolatency option-string=scenecut=0 bitrate=2000 ! f.videosink audiotestsrc ! opusenc ! f.audiosink ftlsink name=f stream-key=<mixer-stream-key>
 * ]|
 * Simple pipeline to stream to mixer.com
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstftlsink.h"

#include "gstftlenums.h"
#include "gstftlvideosink.h"
#include "gstftlaudiosink.h"
#include <inttypes.h>

#define STATUS_POLL_RATE_MS 200

GST_DEBUG_CATEGORY_STATIC (gst_debug_ftl_sink);
#define GST_CAT_DEFAULT gst_debug_ftl_sink

static GQuark ftl_stats_id;

struct _GstFtlSink
{
  GstBin parent_instance;

  gchar *ingest_hostname;
  gchar *stream_key;
  gboolean sync;
  guint peak_kbps;

  GstPad *audiosinkpad;
  GstPad *videosinkpad;

  GstElement *ftlvideosink;
  GstElement *ftlaudiosink;

  ftl_handle_t handle;
  GMutex connect_lock;

  gboolean async_connect;
  gboolean connected;

  GstTask *status_task;
  GRecMutex status_lock;
};

static void gst_ftl_sink_finalize (GObject * object);
static void gst_ftl_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_ftl_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);
static GstStateChangeReturn gst_ftl_sink_change_state (GstElement * element,
    GstStateChange transition);
static void gst_ftl_sink_status_loop (gpointer user_data);
static void gst_ftl_sink_handle_event (GstFtlSink * self,
    ftl_status_event_msg_t * event);

enum
{
  SIGNAL_GET_STATS,
  N_SIGNALS,
};

enum
{
  PROP_0,
  PROP_ASYNC_CONNECT,
  PROP_SYNC,
  PROP_INGEST_HOSTNAME,
  PROP_STREAM_KEY,
  PROP_PEAK_KBPS,
  N_PROPERTIES,
};

static guint G_GNUC_UNUSED signals[N_SIGNALS] = { 0, };
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

#define gst_ftl_sink_parent_class parent_class
G_DEFINE_TYPE (GstFtlSink, gst_ftl_sink, GST_TYPE_BIN);

/* pad templates */

static GstStaticPadTemplate gst_ftl_sink_audio_template =
GST_STATIC_PAD_TEMPLATE ("audiosink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_FTL_AUDIO_SINK_CAPS)
    );
static GstStaticPadTemplate gst_ftl_sink_video_template =
GST_STATIC_PAD_TEMPLATE ("videosink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_FTL_VIDEO_SINK_CAPS)
    );

/* class initialization */

static void
gst_ftl_sink_class_init (GstFtlSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_debug_ftl_sink, "ftlsink", 0,
      "debug category for ftlsink element");

  ftl_stats_id = g_quark_from_static_string ("ftl-stats");

  gst_element_class_set_metadata (element_class,
      "FTL Sink", "Sink",
      "Send to Mixer using the Faster Than Light (FTL) streaming protocol",
      "Make.TV, Inc. <info@make.tv>");

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_ftl_sink_video_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_ftl_sink_audio_template);

  gobject_class->set_property = gst_ftl_sink_set_property;
  gobject_class->get_property = gst_ftl_sink_get_property;
  gobject_class->finalize = gst_ftl_sink_finalize;

  properties[PROP_ASYNC_CONNECT] = g_param_spec_boolean ("async-connect",
      "Async connect", "Connect on PAUSED, otherwise on first push", TRUE,
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
      GST_PARAM_MUTABLE_READY);

  properties[PROP_SYNC] = g_param_spec_boolean ("sync", "Sync",
      "Sync on the clock", TRUE,
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_INGEST_HOSTNAME] = g_param_spec_string ("ingest-hostname",
      "Ingest hostname", "Hostname to connect to", "auto",
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_STREAM_KEY] = g_param_spec_string ("stream-key", "Stream key",
      "Stream key of target channel", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PEAK_KBPS] = g_param_spec_uint ("peak-kbps", "Peak bitrate",
      "Bitrate in kbit/sec to pace outgoing packets", 0, G_MAXINT, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_ftl_sink_change_state);

  GST_DEBUG_REGISTER_FUNCPTR (gst_ftl_sink_status_loop);
}

static void
gst_ftl_sink_init (GstFtlSink * self)
{
  GstPad *pad_audio, *pad_video;

  self->ftlvideosink =
      g_object_new (GST_TYPE_FTL_VIDEO_SINK, "name", "videosink", NULL);
  g_assert_nonnull (self->ftlvideosink);
  self->ftlaudiosink =
      g_object_new (GST_TYPE_FTL_AUDIO_SINK, "name", "audiosink", NULL);
  g_assert_nonnull (self->ftlaudiosink);

  gst_bin_add_many (GST_BIN (self), self->ftlvideosink, self->ftlaudiosink,
      NULL);

  pad_video = gst_element_get_static_pad (self->ftlvideosink, "sink");
  pad_audio = gst_element_get_static_pad (self->ftlaudiosink, "sink");

  self->videosinkpad =
      gst_ghost_pad_new_from_template ("videosink", pad_video,
      gst_static_pad_template_get (&gst_ftl_sink_video_template));
  self->audiosinkpad =
      gst_ghost_pad_new_from_template ("audiosink", pad_audio,
      gst_static_pad_template_get (&gst_ftl_sink_audio_template));

  gst_element_add_pad (GST_ELEMENT_CAST (self), self->videosinkpad);
  gst_element_add_pad (GST_ELEMENT_CAST (self), self->audiosinkpad);

  g_object_bind_property (self, "sync", self->ftlvideosink, "sync",
      G_BINDING_DEFAULT);
  g_object_bind_property (self, "sync", self->ftlaudiosink, "sync",
      G_BINDING_DEFAULT);

  self->status_task = gst_task_new (gst_ftl_sink_status_loop, self, NULL);
  g_rec_mutex_init (&self->status_lock);
  gst_task_set_lock (self->status_task, &self->status_lock);

  g_mutex_init (&self->connect_lock);
}

static void
gst_ftl_sink_finalize (GObject * object)
{
  GstFtlSink *self = GST_FTL_SINK (object);

  g_clear_object (&self->status_task);
  g_rec_mutex_clear (&self->status_lock);

  g_mutex_clear (&self->connect_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_ftl_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFtlSink *self = GST_FTL_SINK (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_ASYNC_CONNECT:
      self->async_connect = g_value_get_boolean (value);
      break;

    case PROP_SYNC:
      self->sync = g_value_get_boolean (value);
      break;

    case PROP_INGEST_HOSTNAME:
      g_free (self->ingest_hostname);
      self->ingest_hostname = g_value_dup_string (value);
      break;

    case PROP_STREAM_KEY:
      g_free (self->stream_key);
      self->stream_key = g_value_dup_string (value);
      break;

    case PROP_PEAK_KBPS:
      self->peak_kbps = g_value_get_uint (value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_ftl_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFtlSink *self = GST_FTL_SINK (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_ASYNC_CONNECT:
      g_value_set_boolean (value, self->async_connect);
      break;
    case PROP_SYNC:
      g_value_set_boolean (value, self->sync);
      break;
    case PROP_INGEST_HOSTNAME:
      g_value_set_string (value, self->ingest_hostname);
      break;

    case PROP_STREAM_KEY:
      g_value_set_string (value, self->stream_key);
      break;

    case PROP_PEAK_KBPS:
      g_value_set_uint (value, self->peak_kbps);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static ftl_status_t
gst_ftl_sink_create_ingest (GstFtlSink * self)
{
  ftl_ingest_params_t params;
  ftl_status_t status_code;

  GST_OBJECT_LOCK (self);
  params.ingest_hostname = self->ingest_hostname;
  params.stream_key = self->stream_key;
  params.video_codec = FTL_VIDEO_H264;
  params.audio_codec = FTL_AUDIO_OPUS;
  params.peak_kbps = self->peak_kbps;
  params.fps_num = 0;
  params.fps_den = 1;
  params.vendor_name = PACKAGE_NAME;
  params.vendor_version = VERSION;

  status_code = ftl_ingest_create (&self->handle, &params);
  GST_OBJECT_UNLOCK (self);

  return status_code;
}

gboolean
gst_ftl_sink_connect (GstFtlSink * self)
{
  gboolean connected;

  g_mutex_lock (&self->connect_lock);

  connected = self->connected;
  if (!connected) {
    ftl_status_t status_code = ftl_ingest_connect (&self->handle);
    if (status_code == FTL_SUCCESS)
      connected = self->connected = TRUE;
    else
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_WRITE,
          ("Failed to connect to ingest: %s",
              ftl_status_code_to_string (status_code)), ("status code %d",
              status_code));
  }

  g_mutex_unlock (&self->connect_lock);
  return connected;
}

static gboolean
gst_ftl_sink_disconnect (GstFtlSink * self)
{
  gboolean connected;

  g_mutex_lock (&self->connect_lock);

  connected = self->connected;
  if (connected) {
    ftl_status_t status_code = ftl_ingest_disconnect (&self->handle);
    if (status_code == FTL_SUCCESS)
      connected = self->connected = FALSE;
    else
      GST_ERROR_OBJECT (self, "Failed to disconnect from ingest: %s",
          ftl_status_code_to_string (status_code));
  }

  g_mutex_unlock (&self->connect_lock);
  return !connected;
}

static GstStateChangeReturn
gst_ftl_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstFtlSink *self = GST_FTL_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  ftl_status_t status_code;
  gboolean async;

  GST_DEBUG_OBJECT (self, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      status_code = gst_ftl_sink_create_ingest (self);
      if (status_code != FTL_SUCCESS) {
        GST_ERROR_OBJECT (self, "Failed to create ingest handle: %s\n",
            ftl_status_code_to_string (status_code));
        return GST_STATE_CHANGE_FAILURE;
      }
      break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* Start retrieving status messages */
      if (!gst_task_start (self->status_task)) {
        GST_ERROR_OBJECT (self, "Failed to start status task");
        return GST_STATE_CHANGE_FAILURE;
      }

      GST_OBJECT_LOCK (self);
      async = self->async_connect;
      GST_OBJECT_UNLOCK (self);

      if (async && !gst_ftl_sink_connect (self)) {
        return GST_STATE_CHANGE_FAILURE;
      }
      break;

    default:
      break;
  }

  /* FIXME: ret gets GST_STATE_CHANGE_SUCCESS even when the stream key is
     invalid.  We might want to use GST_ERROR, or g_error to make it fatal?
   */
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (!gst_ftl_sink_disconnect (self)) {
        return GST_STATE_CHANGE_FAILURE;
      }

      if (!gst_task_join (self->status_task)) {
        GST_ERROR_OBJECT (self, "Failed to join status task");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;

    case GST_STATE_CHANGE_READY_TO_NULL:
      if ((status_code = ftl_ingest_destroy (&self->handle)) != FTL_SUCCESS) {
        GST_ERROR_OBJECT (self, "Failed to destroy ingest handle: %s",
            ftl_status_code_to_string (status_code));
        return GST_STATE_CHANGE_FAILURE;
      }

    default:
      break;
  }

  return ret;
}

static void
gst_ftl_sink_status_loop (gpointer user_data)
{
  GstFtlSink *self = user_data;
  ftl_status_t status_code;
  ftl_status_msg_t message = { FTL_STATUS_NONE, };
  GstStructure *stats_message = NULL;

  GST_TRACE_OBJECT (self, "Getting status");
  status_code = ftl_ingest_get_status (&self->handle, &message,
      STATUS_POLL_RATE_MS);

  while (status_code == FTL_SUCCESS) {
    switch (message.type) {
      case FTL_STATUS_LOG:{
        ftl_status_log_msg_t *msg = &message.msg.log;
        GstDebugLevel level;

        level = gst_ftl_log_severity_to_level (msg->log_level);
        g_strchomp (msg->string);

        GST_CAT_LEVEL_LOG (GST_CAT_DEFAULT, level, self, "%s", msg->string);
        break;
      }

      case FTL_STATUS_EVENT:
        gst_ftl_sink_handle_event (self, &message.msg.event);
        break;

        /* Really for both streams */
      case FTL_STATUS_VIDEO_PACKETS:{
        ftl_packet_stats_msg_t *msg = &message.msg.pkt_stats;

        GST_LOG_OBJECT (self, "Packet stats: period %" PRId64 " ms, %" PRId64
            " packets sent, %" PRId64 " NACK requests, %" PRId64
            " packets lost, %" PRId64 " packets recovered, %" PRId64
            " packets late", msg->period, msg->sent, msg->nack_reqs, msg->lost,
            msg->recovered, msg->late);

        if (stats_message == NULL)
          stats_message = gst_structure_new_id_empty (ftl_stats_id);

        gst_structure_set (stats_message,
            "time-total", GST_TYPE_CLOCK_TIME, msg->period * GST_MSECOND,
            "packets-sent", G_TYPE_INT64, msg->sent,
            "nacks-received", G_TYPE_INT64, msg->nack_reqs, NULL);
        break;
      }

        /* Really for both streams */
      case FTL_STATUS_VIDEO_PACKETS_INSTANT:{
        ftl_packet_stats_instant_msg_t *msg = &message.msg.ipkt_stats;

        GST_LOG_OBJECT (self, "Instant packet stats: period %" PRId64
            " ms, RTT %d ms (min %d ms, max %d ms), delay %d ms (min %d"
            " ms, max %d ms)", msg->period, msg->avg_rtt, msg->min_rtt,
            msg->max_rtt, msg->avg_xmit_delay, msg->min_xmit_delay,
            msg->max_xmit_delay);

        if (stats_message == NULL)
          stats_message = gst_structure_new_id_empty (ftl_stats_id);

        gst_structure_set (stats_message,
            "time-interval", GST_TYPE_CLOCK_TIME, msg->period * GST_MSECOND,
            "rtt-min", G_TYPE_INT, msg->min_rtt,
            "rtt-max", G_TYPE_INT, msg->max_rtt,
            "rtt-avg", G_TYPE_INT, msg->avg_rtt,
            "xmit-delay-min", G_TYPE_INT, msg->min_xmit_delay,
            "xmit-delay-max", G_TYPE_INT, msg->max_xmit_delay,
            "xmit-delay-avg", G_TYPE_INT, msg->avg_xmit_delay, NULL);
        break;
      }

        /* Really just video, this time */
      case FTL_STATUS_VIDEO:{
        ftl_video_frame_stats_msg_t *msg = &message.msg.video_stats;

        GST_LOG_OBJECT (self, "Video frame stats: period %" PRId64
            " ms, %" PRId64 " frames queued, %" PRId64 " frames sent, %" PRId64
            " bytes queued, %" PRId64 " bytes sent, %" PRId64
            " bandwidth throttles, queue fill level %d, max frame size %d",
            msg->period, msg->frames_queued, msg->frames_sent,
            msg->bytes_queued, msg->bytes_sent, msg->bw_throttling_count,
            msg->queue_fullness, msg->max_frame_size);

        if (stats_message == NULL)
          stats_message = gst_structure_new_id_empty (ftl_stats_id);

        gst_structure_set (stats_message,
            "video-frames-queued", G_TYPE_INT64, msg->frames_queued,
            "video-frames-sent", G_TYPE_INT64, msg->frames_sent,
            "video-bytes-queued", G_TYPE_INT64, msg->bytes_queued,
            "video-bytes-sent", G_TYPE_INT64, msg->bytes_sent,
            "video-queue-level", G_TYPE_INT, msg->queue_fullness,
            "video-max-frame-size", G_TYPE_INT, msg->max_frame_size, NULL);
        break;
      }

      case FTL_BITRATE_CHANGED:{
        ftl_bitrate_changed_msg_t *msg = &message.msg.bitrate_changed_msg;
        gdouble nack_value = msg->nacks_to_frames_ratio;
        const gchar *nack_unit = "nacks per frame";

        if (msg->nacks_to_frames_ratio > 0 && msg->nacks_to_frames_ratio < 1) {
          nack_value = 1 / nack_value;
          nack_unit = "frames per nack";
        }

        GST_LOG_OBJECT (self, "Bitrate change: type %s, reason %s, %" PRIu64
            " bps current, %" PRIu64 " bps previous, %.3f %s, RTT %.3f ms,"
            " %" PRIu64 " frames dropped, queue fill level %.3f",
            gst_ftl_bitrate_changed_type_get_nick (msg->bitrate_changed_type),
            gst_ftl_bitrate_changed_reason_get_nick
            (msg->bitrate_changed_reason), msg->current_encoding_bitrate,
            msg->previous_encoding_bitrate, nack_value, nack_unit, msg->avg_rtt,
            msg->avg_frames_dropped, msg->queue_fullness);
        break;
      }

      case FTL_STATUS_NONE:
      case FTL_STATUS_AUDIO_PACKETS:
      case FTL_STATUS_AUDIO:
      case FTL_STATUS_FRAMES_DROPPED:
      case FTL_STATUS_NETWORK:
      default:
        GST_WARNING_OBJECT (self, "Unhandled status message type: %s (%d)",
            gst_ftl_status_type_get_nick (message.type), message.type);
        break;
    }

    GST_TRACE_OBJECT (self, "Getting more status");
    status_code = ftl_ingest_get_status (&self->handle, &message, 0);
  }

  switch (status_code) {
    case FTL_STATUS_TIMEOUT:
      GST_TRACE_OBJECT (self, "Status queue empty");
      break;
    default:
      GST_WARNING_OBJECT (self, "Failed to get status: %s",
          ftl_status_code_to_string (status_code));
      break;
  }

  if (stats_message != NULL)
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_element (GST_OBJECT (self), stats_message));
}

static void
gst_ftl_sink_handle_event (GstFtlSink * self, ftl_status_event_msg_t * event)
{
  GST_INFO_OBJECT (self, "Event: %s, reason %s: %s",
      gst_ftl_status_event_type_get_nick (event->type),
      gst_ftl_status_event_reason_get_nick (event->reason),
      ftl_status_code_to_string (event->error_code));

  if (event->type == FTL_STATUS_EVENT_TYPE_DISCONNECTED &&
      event->reason != FTL_STATUS_EVENT_REASON_API_REQUEST)
    GST_ELEMENT_ERROR_WITH_DETAILS (self, RESOURCE, FAILED,
        ("FTL connection unexpectedly terminated"),
        ("Reason %s: %s",
            gst_ftl_status_event_reason_get_nick (event->reason),
            ftl_status_code_to_string (event->error_code)),
        ("reason", G_TYPE_INT, event->reason,
            "error-code", G_TYPE_INT, event->error_code, NULL));
}

ftl_handle_t *
gst_ftl_sink_get_handle (GstFtlSink * self)
{
  return &self->handle;
}
