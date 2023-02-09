#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>

GST_DEBUG_CATEGORY(appsrc_pipeline_debug);
#define GST_CAT_DEFAULT appsrc_pipeline_debug

struct buffer_lock {
	pthread_mutex_t buffer_lock;
	pthread_cond_t buffer_cond;
	int buffer_ready;
};

struct app_env
{
	GstElement *pipeline;
	GstElement *appsrc;

	GMainLoop *loop;
	guint sourceid;

	GTimer *timer;

	struct buffer_lock bl;
};

struct app_env env;

static gboolean
read_data(struct app_env *app)
{
	guint len;
	GstFlowReturn ret;
	gdouble ms;
	static int early_count = 0, elapsed_count = 0;
	static GstClockTime timestamp = 0;

//	fprintf(stderr, "XXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
	ms = g_timer_elapsed(app->timer, NULL);
	if (ms > 1.0/30.0) {
		GstBuffer *buffer;
		guint8 *pb;
		gboolean ok = TRUE;
		size_t bs = 640 * 480 * 2;

		elapsed_count += 1;

		//pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 640, 480);
		//gdk_pixbuf_fill(pb, 0xffffffff);
		pb = malloc(bs);
		if (!pb) {
			fprintf(stderr, "%s: malloc: %s.\n",
				__func__, strerror(errno));
			ok = FALSE;
			goto malloc_error;
		}
		memset(pb, 1 << (elapsed_count / 4), bs);


		//GST_BUFFER_DATA(buffer) = gdk_pixbuf_get_pixels(pb);
		//GST_BUFFER_SIZE(buffer) = 640*480*3*sizeof(guchar);
		buffer = gst_buffer_new_wrapped_full(0, pb, bs, 0, bs,
						     NULL, NULL );
		GST_DEBUG(">>>>>>>>>>>>>>>>>> feed buffer");

		GST_BUFFER_PTS(buffer) = timestamp;
		GST_BUFFER_DURATION(buffer) =
			gst_util_uint64_scale_int(1, 30, 1);

		timestamp += GST_BUFFER_DURATION(buffer);

		g_signal_emit_by_name(app->appsrc, "push-buffer", buffer, &ret);
		gst_buffer_unref(buffer);

		if (ret != GST_FLOW_OK) {
			/* some error, stop sending data */
			GST_DEBUG("some error");
			ok = FALSE;
		}

		if (elapsed_count >= 30)
			gst_app_src_end_of_stream((GstAppSrc *)app->appsrc);

	malloc_error:
		g_timer_start(app->timer);

		return ok;
	} else {
		early_count += 1;
	}

	// g_signal_emit_by_name(app->appsrc, "end-of-stream", &ret);
	return TRUE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
* to the mainloop to start pushing data into the appsrc */
static void
start_feed(GstElement *pipeline, guint size, struct app_env *app)
{
	if (app->sourceid == 0) {
		GST_DEBUG("start feeding");
		app->sourceid = g_idle_add((GSourceFunc) read_data, app);
	}
}

/* This callback is called when appsrc has enough data and we can stop sending.
* We remove the idle handler from the mainloop */
static void
stop_feed(GstElement *pipeline, struct app_env *app)
{
	if (app->sourceid != 0) {
		GST_DEBUG("stop feeding");
		g_source_remove(app->sourceid);
		app->sourceid = 0;
	}
}

static gboolean
bus_message(GstBus *bus, GstMessage *message, struct app_env *app)
{
	GST_DEBUG("got message %s",
		  gst_message_type_get_name(GST_MESSAGE_TYPE(message)));

	switch(GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_ERROR: {
		GError *err = NULL;
		gchar *dbg_info = NULL;

		gst_message_parse_error(message, &err, &dbg_info);
		g_printerr("ERROR from element %s: %s\n",
			   GST_OBJECT_NAME(message->src), err->message);
		g_printerr("Debugging info: %s\n",(dbg_info) ? dbg_info : "none");
		g_error_free(err);
		g_free(dbg_info);
		g_main_loop_quit(app->loop);
		break;
	}
	case GST_MESSAGE_EOS:
		g_main_loop_quit(app->loop);
		break;
	default:
		break;
	}
	return TRUE;
}

int
main(int argc, char *argv[])
{
	struct app_env *app = &env;
	GError *error = NULL;
	GstBus *bus;
	GstCaps *caps;

	gst_init(&argc, &argv);

	GST_DEBUG_CATEGORY_INIT(appsrc_pipeline_debug, "appsrc-pipeline", 0,
				"appsrc pipeline example");

	/* create a mainloop to get messages and to handle the idle
	 * handler that will feed data to appsrc. */
	app->loop = g_main_loop_new(NULL, TRUE);
	app->timer = g_timer_new();

	// Option 1: Display on screen via xvimagesink
	app->pipeline =
		gst_parse_launch("appsrc name=mysource is-live=true block=true "
				 "caps=video/x-raw,width=640,height=480,"
				 "format=YUY2,framerate=30/1 "
//				 "! videoconvert "
				 "! autovideosink", NULL);

	g_assert(app->pipeline);

	bus = gst_pipeline_get_bus(GST_PIPELINE(app->pipeline));
	g_assert(bus);

	/* add watch for messages */
	gst_bus_add_watch(bus, (GstBusFunc)bus_message, app);

	/* get the appsrc */
	app->appsrc = gst_bin_get_by_name(GST_BIN(app->pipeline), "mysource");

	g_assert(app->appsrc);
	g_assert(GST_IS_APP_SRC(app->appsrc));

	g_signal_connect(app->appsrc, "need-data", G_CALLBACK(start_feed), app);
	g_signal_connect(app->appsrc, "enough-data", G_CALLBACK(stop_feed), app);

	/* set the caps on the source */
//	caps = gst_video_format_new_caps(GST_VIDEO_FORMAT_RGB, 640, 480, 0, 1, 4, 3);
//	gst_app_src_set_caps(GST_APP_SRC(app->appsrc), caps);


	/* go to playing and wait in a mainloop. */
	gst_element_set_state(app->pipeline, GST_STATE_PLAYING);

	/* this mainloop is stopped when we receive an error or EOS */
	g_main_loop_run(app->loop);

	GST_DEBUG("stopping");

	gst_element_set_state(app->pipeline, GST_STATE_NULL);

	gst_object_unref(bus);
	g_main_loop_unref(app->loop);

	return 0;
}
