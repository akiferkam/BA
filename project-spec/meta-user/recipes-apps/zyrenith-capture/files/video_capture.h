#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <stddef.h>

#define VIDEO_CAPTURE_MAX_BUFFERS 8

typedef struct {
	void *start;
	size_t length;
} video_capture_buffer_t;

typedef struct {
	int fd;
	video_capture_buffer_t buffers[VIDEO_CAPTURE_MAX_BUFFERS];
	unsigned int buffer_count;
	unsigned int width;
	unsigned int height;
	unsigned int streaming;
} video_capture_t;

/* Opens the V4L2 device, negotiates MJPG format at width/height/fps,
 * and mmaps the driver's capture buffers. Returns 0 on success. */
int video_capture_open(video_capture_t *vc, const char *device,
			unsigned int width, unsigned int height,
			unsigned int fps);

/* Queues all buffers and starts streaming (VIDIOC_STREAMON). */
int video_capture_start(video_capture_t *vc);

/* Dequeues one filled buffer. On success *data/*length point at the
 * captured frame and *buf_index identifies the buffer for release. */
int video_capture_grab_frame(video_capture_t *vc, void **data,
			      size_t *length, unsigned int *buf_index);

/* Re-queues a buffer previously returned by video_capture_grab_frame. */
int video_capture_release_frame(video_capture_t *vc, unsigned int buf_index);

/* Stops streaming (VIDIOC_STREAMOFF). */
int video_capture_stop(video_capture_t *vc);

/* Unmaps buffers and closes the device. */
void video_capture_close(video_capture_t *vc);

#endif /* VIDEO_CAPTURE_H */
