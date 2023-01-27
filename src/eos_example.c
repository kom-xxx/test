#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

#include <gst/gst.h>


static volatile int interrupted = 0;

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
}

int
main(int   argc, char *argv[])
{
	GMainLoop *loop;
	GstElement *pipeline;
	pthread_t thread;

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

	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	g_main_loop_run(loop);
	gst_element_set_state(pipeline, GST_STATE_NULL);

	pthread_join(thread, NULL);

	gst_object_unref(GST_OBJECT(pipeline));
	g_main_loop_unref(loop);

	return 0;
}
