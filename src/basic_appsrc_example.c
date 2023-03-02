#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>
#include <queue>
#include <mutex>
#include<cstdio>
#include <chrono>
#include <cstdlib>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
std::queue<cv::Mat> q1;
using namespace cv;
using namespace std;
mutex my_mutex;
static GMainLoop *loop;

void process20(queue<cv::Mat>& q1) {
	for (int i = 0; i < 100; i\+\+) {
		Mat srcImg;
		srcImg = imread("/media/sd-mmcblk1p2/opt/gray.bmp", 0);
		if(srcImg.data==0)
		{
			cout << "fliead" << endl;
		}
		cvtColor(srcImg, srcImg, CV_GRAY2BGR);
		cvtColor(srcImg,srcImg, CV_BGR2YUV_I420);
		my_mutex.lock();
		q1.push(srcImg);
		my_mutex.unlock();
	}
}
/* 线程一 */
static void
cb_need_data_1(GstElement *appsrc,
	       guint unused_size,
	       gpointer user_data)
{
	printf("1111\n");
	static GstClockTime timestamp = 0;
	GstBuffer *buffer;
	GstFlowReturn ret;
	GstMapInfo map;

	my_mutex.lock();
	size_t sizeInBytes =q1.front().total() * q1.front().elemSize();

	buffer=gst_buffer_new_allocate(NULL,sizeInBytes,NULL);

	gst_buffer_map(buffer,&map,GST_MAP_WRITE);
	memcpy(map.data,q1.front().data,sizeInBytes);
//cout << "q1.size=" << q1.size() << endl;
	q1.pop();
	my_mutex.unlock();
	GST_BUFFER_PTS(buffer) = timestamp;
	GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND,10);
	timestamp += GST_BUFFER_DURATION(buffer);
	gst_buffer_unmap(buffer,&map);

	g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
	gst_buffer_unref(buffer);
	if(ret!=GST_FLOW_OK)
	{
		g_main_loop_quit(loop);
	}
}

gint
main (gint argc, gchar *argv[])
{
	process20(q1);//图像处理后入队
	GstElement *pipeline, *appsrc,*enc,*sink;
	gst_init (&argc, &argv);
	loop = g_main_loop_new (NULL, FALSE);
	appsrc=gst_element_factory_make ("appsrc", "src");
//videoparse = gst_element_factory_make ("rawvideoparse", "rawvideoparse");
	enc=gst_element_factory_make("omxh265enc","video_enc");
	sink=gst_element_factory_make("filesink","filesink");
	pipeline = gst_pipeline_new ("pipeline");
//g_object_set(source, "location","/media/mytest/out.yuv", NULL);
//设置appsrc
	g_object_set(G_OBJECT(appsrc),
		     "caps",gst_caps_new_simple("video/x-raw",
						"format", G_TYPE_STRING, "NV12",
						"width", G_TYPE_INT, 2044,
						"height", G_TYPE_INT, 1496,
						"framerate", GST_TYPE_FRACTION, 10, 1,
						NULL), NULL);
	g_object_set(G_OBJECT(appsrc), "is-live","true", NULL);
	g_object_set(G_OBJECT(appsrc), "block","true", NULL);
// g_object_set(G_OBJECT(videoparse), "format",23, NULL);
// g_object_set(G_OBJECT(videoparse), "width",2044, NULL);
// g_object_set(G_OBJECT(videoparse), "height",1496, NULL);
// g_object_set(G_OBJECT(videoparse), "framerate",10/1, NULL);
	g_object_set(G_OBJECT(sink), "location","/media/sd-mmcblk1p2/opt/14.h265", NULL);
	gst_bin_add_many (GST_BIN (pipeline), appsrc,enc,sink, NULL);
	gst_element_link_many (appsrc, enc,sink,NULL);
/* setup appsrc */
	g_object_set(G_OBJECT(appsrc),
		     "stream-type", 0,
		     "format", GST_FORMAT_TIME,
		     NULL);
// play
	g_signal_connect(appsrc, "need-data", G_CALLBACK(cb_need_data_1), NULL);
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	g_main_loop_run (loop);
// clean
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (pipeline));
	g_main_loop_unref (loop);

	return 0;
}


