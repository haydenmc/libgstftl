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

#ifndef _GST_FTL_ENUMS_H_
#define _GST_FTL_ENUMS_H_

#include <gst/gst.h>
#include "ftl.h"

G_BEGIN_DECLS

GstDebugLevel gst_ftl_log_severity_to_level (ftl_log_severity_t value);
const gchar * gst_ftl_status_type_get_nick (ftl_status_types_t value);
const gchar * gst_ftl_status_event_type_get_nick (ftl_status_event_types_t value);
const gchar * gst_ftl_status_event_reason_get_nick (ftl_status_event_reasons_t value);
const gchar * gst_ftl_bitrate_changed_type_get_nick (ftl_bitrate_changed_type_t value);
const gchar * gst_ftl_bitrate_changed_reason_get_nick (ftl_bitrate_changed_reason_t value);

G_END_DECLS

#endif
