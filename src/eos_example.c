#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

#include <gst/gst.h>


static volatile int interrupted = 0;

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
	GMainLoop *loop = (GMainLoop *) data;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		fprintf(stderr, "%s: got EOS\n", __func__);

		g_main_loop_quit(loop);
		break;
	case GST_MESSAGE_ERROR: {
		gchar  *debug;
		GError *error;

		gst_message_parse_error (msg, &error, &debug);
		g_free(debug);

		fprintf(stderr, "%s: %s\n", __func__, error->message);
		g_error_free(error);

		g_main_loop_quit(loop);
		break;
	}
	default:
		break;
	}

	return TRUE;
}

static void
intr_hdlr(int sig)
{
	if (!interrupted)
		interrupted = 1;
	else
		_exit(1);
}

static void *thr(void *arg)
{
	GstElement *pipeline = arg;

	while (!interrupted)
		usleep(1000);

	gst_element_send_event(pipeline, gst_event_new_eos ());
	fprintf(stderr, "%s: EOS has heen sent\n", __func__);

	return NULL;
}

int
main(int   argc, char *argv[])
{
	GMainLoop *loop;
	GstBus *bus;
	GstElement *pipeline;
	pthread_t thread;
	gint bus_watch_id;

	signal(SIGINT, intr_hdlr);

	/* Initialisation */
	gst_init(&argc, &argv);

	loop = g_main_loop_new(NULL, FALSE);

	pipeline = gst_parse_launch("v4l2src device=/dev/video0 "
				    "! video/x-raw,width=640,height=480,"
				    "framerate=30/1,format=YUY2"
				    "! videoconvert"
				    "! x264enc"
				    "! mp4mux"
				    "! filesink location=output/camera.mp4",
				    NULL);

	pthread_create(&thread, NULL, thr, pipeline);

	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
	gst_object_unref(bus);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	g_main_loop_run(loop);
	fprintf(stderr, "%s: exited main_loop\n", __func__);
	gst_element_set_state(pipeline, GST_STATE_NULL);

	pthread_join(thread, NULL);
	fprintf(stderr, "%s: thread:%lu joined\n", __func__, thread);

	gst_object_unref(GST_OBJECT(pipeline));
	g_source_remove(bus_watch_id);
	g_main_loop_unref(loop);

	return 0;
}
