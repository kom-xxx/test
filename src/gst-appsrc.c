// example appsrc for gstreamer 1.0 with own mainloop & external buffers. based on example from gstreamer docs.
// public domain, 2015 by Florian Echtler <floe@butterbrot.org>. compile with:
// gcc --std=c99 -Wall $(pkg-config --cflags gstreamer-1.0) -o gst-appsrc gst-appsrc.c $(pkg-config --libs gstreamer-1.0) -lgstapp-1.0

#include <sys/time.h>

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "gst-appsrc.h"

/* #define WITH_MAIN */
#undef WITH_MP4ENC
#define WITH_MP4MUX
#define WITH_FILESINK
#define MEASURE_INTERVAL

#ifndef __FreeBSD__
#define	timespecclear(tvp)	((tvp)->tv_sec = (tvp)->tv_nsec = 0)
#define	timespecisset(tvp)	((tvp)->tv_sec || (tvp)->tv_nsec)
#define	timespeccmp(tvp, uvp, cmp)					\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_nsec cmp (uvp)->tv_nsec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))

#define	timespecadd(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec >= 1000000000L) {			\
			(vsp)->tv_sec++;				\
			(vsp)->tv_nsec -= 1000000000L;			\
		}							\
	} while (0)
#define	timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (0)
#endif

struct gst_mng {
	GstElement *pipeline;
	GstAppSrc *appsrc;
};

pthread_mutex_t busy_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t busy_wait = PTHREAD_COND_INITIALIZER;
volatile int busy = 0;

static void
push_buffer(GstAppSrc* appsrc, void *image, gsize size)
{
	static GstClockTime timestamp = 0;
	GstBuffer *buffer;
	GstFlowReturn ret;

	size = 384 * 288 * 2;

	buffer = gst_buffer_new_wrapped_full(0, image, size, 0, size,
					     NULL, NULL );

	GST_BUFFER_PTS(buffer) = timestamp;
	GST_BUFFER_DURATION(buffer) =
		gst_util_uint64_scale_int(1, GST_SECOND, 4);

	timestamp += GST_BUFFER_DURATION(buffer);

	ret = gst_app_src_push_buffer(appsrc, buffer);

	if (ret != GST_FLOW_OK) {
		/* something wrong, stop pushing */
		// g_main_loop_quit (loop);
	}
}

static void
cb_need_data(GstElement *appsrc, guint unused_size, gpointer user_data)
{
	//push_buffer((GstAppSrc*)appsrc);
	pthread_mutex_lock(&busy_lock);
	busy = 0;
	pthread_cond_signal(&busy_wait);
	pthread_mutex_unlock(&busy_lock);
	fprintf(stdout, "%s: rset busy\n", __func__);
}

void *
init_gst(void)
{
	struct gst_mng *mng;
	GstElement *pipeline, *appsrc, *conv, *videosink;
#ifdef WITH_MP4ENC
	GstElement *enc, *mux;
#endif
#ifdef USE_PARSER
	GError *error = NULL;
#endif

	mng = calloc(1, sizeof (struct gst_mng));
	if (!mng)
		return NULL;

	/* init GStreamer */
	gst_init (NULL, NULL);

#ifdef USE_PARSER
	pipeline = gst_parse_launch(
		"appsrc "
		"caps=video/xraw,format=rgb16,with=384,height=288,framerate=0/1"
		" stream-type=0 format=3 "
		"! videoconvert"
		"! autovideosink",
		NULL
		);
	appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc0");
	if (!appsrc) {
		fprintf(stderr, "%s: gst_bin_get_byname: %s\n",
			__func__, error->message);
		return 1;
	}
#else  /* !USE_PARSER */
  
	/**
	 ** create pipeline elements
	 **/
	pipeline = gst_pipeline_new ("pipeline");
	appsrc = gst_element_factory_make ("appsrc", "source");
	conv = gst_element_factory_make ("videoconvert", "conv");
# ifdef WITH_MP4ENC
	enc = gst_element_factory_make("avenc_mpeg4", "enc");
#  ifdef WITH_MP4MUX
	mux = gst_element_factory_make("mp4mux", "mux");
#  endif
# endif
# ifdef WITH_FILESINK
	videosink = gst_element_factory_make ("filesink", "videosink");
# else
	videosink = gst_element_factory_make ("autovideosink", "videosink");
# endif

	fprintf(stderr, "%s: pipeline:%p appsrc:%p conv:%p"
# ifdef WITH_MP4ENC
		" enc:%p"
#  ifdef WITH_MP4MUX
		" mux:%p"
#  endif
# endif
		 " videosink:%p\n",
		__func__, pipeline, appsrc,
# ifdef WITH_MP4ENC
		enc,
#  ifdef WITH_MP4MUX
		mux,
#  endif
# endif
		 conv, videosink);

	mng->pipeline = pipeline;
	mng->appsrc = (GstAppSrc *)appsrc;

	/**
	 **  set up properties
	 **/
	g_object_set (G_OBJECT(appsrc), "caps",
		      gst_caps_new_simple("video/x-raw",
					  "format", G_TYPE_STRING, CAM_FORMAT,
					  "width", G_TYPE_INT, CAM_WIDTH,
					  "height", G_TYPE_INT, CAM_HEIGHT,
					  "framerate", GST_TYPE_FRACTION, CAM_FR,
					  NULL), NULL);
# ifdef WITH_FILESINK
#  ifdef WITH_MP4ENC
	g_object_set(G_OBJECT(videosink), "location", "output/frames.mp4", NULL);
#  else
	g_object_set(G_OBJECT(videosink), "location", "output/frames.raw", NULL);
#  endif
# endif

	/**
	 ** connect pipeline elements
	 **/
	gst_bin_add_many(GST_BIN(pipeline), appsrc, conv,
# ifdef WITH_MP4ENC
			 enc,
#  ifdef WITH_MP4MUX
			 mux,
#  endif
# endif
			 videosink, NULL);
	gst_element_link_many(appsrc, conv,
# ifdef WITH_MP4ENC
			      enc,
#  ifdef WITH_MP4MUX
			      mux,
#  endif
# endif
			      videosink, NULL);
	/* setup appsrc */
	g_object_set (G_OBJECT (appsrc),
		      "stream-type", 0, // GST_APP_STREAM_TYPE_STREAM
		      "format", GST_FORMAT_TIME,
		      "is-live", TRUE,
		      NULL);
#endif	/* USE_PARSER */

	/**
	 ** set up bus signal
	 **/
	g_signal_connect (appsrc, "need-data", G_CALLBACK (cb_need_data), NULL);

	/**
	 ** start player
	 **/
	gst_element_set_state (pipeline, GST_STATE_PLAYING);

	return mng;
}

int
push_frame(void *_mng, void *buffer, size_t size)
{
	struct gst_mng *mng = _mng;
	GMainContext *ctx;

	fprintf(stderr, "%s: buffer:%p size:%ld\n", __func__, buffer, size);

#ifdef MEASURE_INTERVAL
	struct timespec now, diff;
	static struct timespec latest = {0L, 0L};

	g_assert(size <= G_MAXSIZE);

	if (!timespecisset(&latest))
	    clock_gettime(CLOCK_MONOTONIC, &latest);
#endif
#if 1
	pthread_mutex_lock(&busy_lock);
	if (busy)
		pthread_cond_wait(&busy_wait, &busy_lock);
	busy = 1;
	pthread_mutex_unlock(&busy_lock);
#endif

#ifdef MEASURE_INTERVAL
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecsub(&now, &latest, &diff);
	fprintf(stderr,
		"%s: latest:%ld.%09ld now:%ld.%09ld diff:%ld.%09ld\n",
		__func__,
		now.tv_sec, now.tv_nsec,
		latest.tv_sec, latest.tv_nsec,
		diff.tv_sec, diff.tv_nsec);
	latest = now;
#endif

	push_buffer((GstAppSrc*)mng->appsrc, buffer, size);

	ctx = g_main_context_default();
	g_main_context_iteration(ctx, FALSE);

	return 0;
}

void
deinit_gst(void *_mng)
{
	struct gst_mng *mng = _mng;

	/* clean up */
	gst_element_set_state(mng->pipeline, GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(mng->pipeline));
	free(mng);
}

#ifdef WITH_MAIN

#define REPEAT_COUNT 1024
#define NR_FRAME 32

uint16_t frames[32][640*480];

int
main(int ac, char **av)
{
	void *gst_handle;

	for (int i = 0; i < 32; i++)
		memset(frames[i], i + 'A', sizeof frames[0]);

	gst_handle = init_gst();
	if (!gst_handle)
		return 1;

	for (int i = 0; i < REPEAT_COUNT; ++i)
		push_frame(gst_handle, frames[i % NR_FRAME], sizeof frames[0]);

	deinit_gst(gst_handle);

	return 0;
}

#endif	/* WITH_MAIN */
