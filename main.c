/*
 *  main.c
 *  KinectSqueak
 *
 *  Created by Nikolay Suslov on 11/24/10.
 *  Copyright 2010 VSTU. All rights reserved.
 *
 * This file contains parts of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#include <stdio.h>
#include <string.h>
#include <libusb.h>
#include "libfreenect.h"

#include <pthread.h>

#include <math.h>

pthread_t freenect_thread;
//pthread_t freenect_threadN;
volatile int die = 0;

int g_argc;
char **g_argv;

int window;

pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t gl_depth_front[640*480*4];
uint8_t gl_depth_back[640*480*4];

uint8_t gl_rgb_front[640*480*4];
uint8_t gl_rgb_back[640*480*4];

freenect_context *f_ctx;
freenect_device *f_dev;
int freenect_angle = 0;
int freenect_led;


pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_frames = 0;




int freenectInit(){
	
if (freenect_init(&f_ctx, NULL) < 0) {
	printf("freenect_init() failed\n");
	return 1;
}
	
	//freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);
	
	int nr_devices = freenect_num_devices (f_ctx);
	printf ("Number of devices found: %d\n", nr_devices);
	
	
	if (freenect_open_device(f_ctx, &f_dev, 0) < 0) {
		printf("Could not open device\n");
		return 1;
	}
	
	return 2;
}



freenect_context* getFreenectContext() {
	return f_ctx;
}



freenect_device* getFreenectDevice() {
	return f_dev;
}


void setLedToRed() {
	freenect_set_led(f_dev,LED_RED);
}


void setLed(freenect_device *dev, int led) {	
	freenect_set_led(dev,led);
}


void setTiltDegs(freenect_device *dev, int deg) {
	
	freenect_set_tilt_degs(dev,deg);
	
}


float test() {
	
	return 10.0;
}

int testKinect(){

	libusb_device_handle *dev;
	
	libusb_init(NULL);
	//libusb_set_debug(0, 3);
	
	dev = libusb_open_device_with_vid_pid(NULL, 0x45e, 0x2ae);
	if (!dev) {
		//printf("Could not open device\n");
		return 1;
	}
	
	return 3;

}




uint16_t t_gamma[2048];

void depth_cb(freenect_device *dev, freenect_depth *v_depth, uint32_t timestamp)
{
	int i;
	freenect_depth *depth = v_depth;
	
	pthread_mutex_lock(&gl_backbuf_mutex);
	for (i=0; i<FREENECT_FRAME_PIX; i++) {
		int pval = t_gamma[depth[i]];
		int lb = pval & 0xff;
		switch (pval>>8) {
			case 0:
				gl_depth_back[3*i+0] = 255;
				gl_depth_back[3*i+1] = 255-lb;
				gl_depth_back[3*i+2] = 255-lb;
				break;
			case 1:
				gl_depth_back[3*i+0] = 255;
				gl_depth_back[3*i+1] = lb;
				gl_depth_back[3*i+2] = 0;
				break;
			case 2:
				gl_depth_back[3*i+0] = 255-lb;
				gl_depth_back[3*i+1] = 255;
				gl_depth_back[3*i+2] = 0;
				break;
			case 3:
				gl_depth_back[3*i+0] = 0;
				gl_depth_back[3*i+1] = 255;
				gl_depth_back[3*i+2] = lb;
				break;
			case 4:
				gl_depth_back[3*i+0] = 0;
				gl_depth_back[3*i+1] = 255-lb;
				gl_depth_back[3*i+2] = 255;
				break;
			case 5:
				gl_depth_back[3*i+0] = 0;
				gl_depth_back[3*i+1] = 0;
				gl_depth_back[3*i+2] = 255-lb;
				break;
			default:
				gl_depth_back[3*i+0] = 0;
				gl_depth_back[3*i+1] = 0;
				gl_depth_back[3*i+2] = 0;
				break;
		}
		
		//gl_depth_back[4 *  i + 3] = 0x00;
		
	}
	got_frames++;
	//memcpy(gl_depth_back, depth, FREENECT_DEPTH_SIZE);
	pthread_cond_signal(&gl_frame_cond);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

void rgb_cb(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp)
{
	pthread_mutex_lock(&gl_backbuf_mutex);
	got_frames++;
	memcpy(gl_rgb_back, rgb, FREENECT_RGB_SIZE);
	pthread_cond_signal(&gl_frame_cond);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

void *freenect_threadfunc(void *arg)
{
	freenect_set_tilt_degs(f_dev,freenect_angle);
	freenect_set_led(f_dev,LED_RED);
	freenect_set_depth_callback(f_dev, depth_cb);
	freenect_set_rgb_callback(f_dev, rgb_cb);
	freenect_set_rgb_format(f_dev, FREENECT_FORMAT_RGB);
	freenect_set_depth_format(f_dev, FREENECT_FORMAT_11_BIT);
	
	freenect_start_depth(f_dev);
	freenect_start_rgb(f_dev);
	
	printf("'w'-tilt up, 's'-level, 'x'-tilt down, '0'-'6'-select LED mode\n");
	
	while(!die && freenect_process_events(f_ctx) >= 0 )
	{
		int16_t ax,ay,az;
		freenect_get_raw_accel(f_dev, &ax, &ay, &az);
		double dx,dy,dz;
		freenect_get_mks_accel(f_dev, &dx, &dy, &dz);
		printf("\r raw acceleration: %4d %4d %4d  mks acceleration: %4f %4f %4f\r", ax, ay, az, dx, dy, dz);
		fflush(stdout);
	}
	
	printf("\nshutting down streams...\n");
	
	freenect_stop_depth(f_dev);
	freenect_stop_rgb(f_dev);
	
	printf("-- done!\n");
	return NULL;
}


void processSensor()
{
	pthread_mutex_lock(&gl_backbuf_mutex);
	
	while (got_frames < 2) {
		pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
	}
	
	memcpy(gl_depth_front, gl_depth_back, sizeof(gl_depth_back));
	memcpy(gl_rgb_front, gl_rgb_back, sizeof(gl_rgb_back));
	got_frames = 0;
	pthread_mutex_unlock(&gl_backbuf_mutex);
	
}


uint8_t* getRGBFront(){

	return gl_rgb_front;

}

uint8_t* getDepthFront(){
	
	return gl_depth_front;
	
}



int mainInit(int argc, char **argv){

	int res;
	
	
	printf("Kinect camera test\n");
	
	int i;
	for (i=0; i<2048; i++) {
		float v = i/2048.0;
		v = powf(v, 3)* 6;
		t_gamma[i] = v*6*256;
	}
	
	g_argc = argc;
	g_argv = argv;
	
	if (freenect_init(&f_ctx, NULL) < 0) {
		printf("freenect_init() failed\n");
		return 1;
	}
	
	//freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);
	
	int nr_devices = freenect_num_devices (f_ctx);
	printf ("Number of devices found: %d\n", nr_devices);
	
	int user_device_number = 0;
	if (argc > 1)
		user_device_number = atoi(argv[1]);
	
	if (nr_devices < 1)
		return 1;
	
	if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
		printf("Could not open device\n");
		return 1;
	}
	
	res = pthread_create(&freenect_thread, NULL, freenect_threadfunc, NULL);
	if (res) {
		printf("pthread_create failed\n");
		return 1;
	}
	
	
	return 0;

}

void stopSensor(){
	
		die = 1;
		//pthread_join(freenect_thread, NULL);
		//pthread_exit(NULL);	

}
