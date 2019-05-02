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

#ifndef _GST_FTL_SINK_H_
#define _GST_FTL_SINK_H_

#include <gst/gst.h>
#include "ftl.h"

G_BEGIN_DECLS

#define GST_TYPE_FTL_SINK gst_ftl_sink_get_type ()
G_DECLARE_FINAL_TYPE (GstFtlSink, gst_ftl_sink, GST, FTL_SINK, GstBin)

ftl_handle_t * gst_ftl_sink_get_handle (GstFtlSink * sink);
gboolean gst_ftl_sink_connect (GstFtlSink * self);

G_END_DECLS

#endif
