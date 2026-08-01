#include "video_capture.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>

static int xioctl(int fd, unsigned long request, void *arg)
{
	int r;

	do {
		r = ioctl(fd, request, arg);
	} while (r == -1 && errno == EINTR);

	return r;
}

int video_capture_open(video_capture_t *vc, const char *device,
			unsigned int width, unsigned int height,
			unsigned int fps)
{
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	struct v4l2_requestbuffers req;
	unsigned int i;

	memset(vc, 0, sizeof(*vc));

	vc->fd = open(device, O_RDWR | O_NONBLOCK);
	if (vc->fd < 0) {
		fprintf(stderr, "video_capture: open(%s): %s\n", device,
			strerror(errno));
		return -1;
	}

	memset(&cap, 0, sizeof(cap));
	if (xioctl(vc->fd, VIDIOC_QUERYCAP, &cap) == -1) {
		fprintf(stderr, "video_capture: VIDIOC_QUERYCAP: %s\n",
			strerror(errno));
		goto fail_close;
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
	    !(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "video_capture: %s lacks capture/streaming\n",
			device);
		goto fail_close;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	if (xioctl(vc->fd, VIDIOC_S_FMT, &fmt) == -1) {
		fprintf(stderr, "video_capture: VIDIOC_S_FMT: %s\n",
			strerror(errno));
		goto fail_close;
	}
	vc->width = fmt.fmt.pix.width;
	vc->height = fmt.fmt.pix.height;

	/* Frame rate must be requested explicitly - the driver otherwise
	 * keeps whatever interval was last negotiated (often a low
	 * default), see bandwidth/latency characterization notes. */
	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = fps;
	if (xioctl(vc->fd, VIDIOC_S_PARM, &parm) == -1) {
		fprintf(stderr, "video_capture: VIDIOC_S_PARM: %s\n",
			strerror(errno));
		/* non-fatal: continue with whatever interval is active */
	}

	memset(&req, 0, sizeof(req));
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (xioctl(vc->fd, VIDIOC_REQBUFS, &req) == -1) {
		fprintf(stderr, "video_capture: VIDIOC_REQBUFS: %s\n",
			strerror(errno));
		goto fail_close;
	}
	if (req.count < 2 || req.count > VIDEO_CAPTURE_MAX_BUFFERS) {
		fprintf(stderr, "video_capture: unexpected buffer count %u\n",
			req.count);
		goto fail_close;
	}
	vc->buffer_count = req.count;

	for (i = 0; i < vc->buffer_count; i++) {
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (xioctl(vc->fd, VIDIOC_QUERYBUF, &buf) == -1) {
			fprintf(stderr, "video_capture: VIDIOC_QUERYBUF: %s\n",
				strerror(errno));
			goto fail_unmap;
		}

		vc->buffers[i].length = buf.length;
		vc->buffers[i].start = mmap(NULL, buf.length,
					     PROT_READ | PROT_WRITE,
					     MAP_SHARED, vc->fd, buf.m.offset);
		if (vc->buffers[i].start == MAP_FAILED) {
			fprintf(stderr, "video_capture: mmap: %s\n",
				strerror(errno));
			vc->buffers[i].start = NULL;
			goto fail_unmap;
		}
	}

	return 0;

fail_unmap:
	for (i = 0; i < vc->buffer_count; i++) {
		if (vc->buffers[i].start)
			munmap(vc->buffers[i].start, vc->buffers[i].length);
	}
fail_close:
	close(vc->fd);
	vc->fd = -1;
	return -1;
}

int video_capture_start(video_capture_t *vc)
{
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < vc->buffer_count; i++) {
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (xioctl(vc->fd, VIDIOC_QBUF, &buf) == -1) {
			fprintf(stderr, "video_capture: VIDIOC_QBUF: %s\n",
				strerror(errno));
			return -1;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(vc->fd, VIDIOC_STREAMON, &type) == -1) {
		fprintf(stderr, "video_capture: VIDIOC_STREAMON: %s\n",
			strerror(errno));
		return -1;
	}

	vc->streaming = 1;
	return 0;
}

int video_capture_grab_frame(video_capture_t *vc, void **data,
			      size_t *length, unsigned int *buf_index)
{
	struct v4l2_buffer buf;
	fd_set fds;
	struct timeval tv;
	int r;

	FD_ZERO(&fds);
	FD_SET(vc->fd, &fds);
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	r = select(vc->fd + 1, &fds, NULL, NULL, &tv);
	if (r == -1) {
		fprintf(stderr, "video_capture: select: %s\n", strerror(errno));
		return -1;
	}
	if (r == 0) {
		fprintf(stderr, "video_capture: frame wait timed out\n");
		return -1;
	}

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (xioctl(vc->fd, VIDIOC_DQBUF, &buf) == -1) {
		fprintf(stderr, "video_capture: VIDIOC_DQBUF: %s\n",
			strerror(errno));
		return -1;
	}

	*data = vc->buffers[buf.index].start;
	*length = buf.bytesused;
	*buf_index = buf.index;
	return 0;
}

int video_capture_release_frame(video_capture_t *vc, unsigned int buf_index)
{
	struct v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = buf_index;
	if (xioctl(vc->fd, VIDIOC_QBUF, &buf) == -1) {
		fprintf(stderr, "video_capture: VIDIOC_QBUF (release): %s\n",
			strerror(errno));
		return -1;
	}
	return 0;
}

int video_capture_stop(video_capture_t *vc)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (!vc->streaming)
		return 0;

	if (xioctl(vc->fd, VIDIOC_STREAMOFF, &type) == -1) {
		fprintf(stderr, "video_capture: VIDIOC_STREAMOFF: %s\n",
			strerror(errno));
		return -1;
	}
	vc->streaming = 0;
	return 0;
}

void video_capture_close(video_capture_t *vc)
{
	unsigned int i;

	for (i = 0; i < vc->buffer_count; i++) {
		if (vc->buffers[i].start)
			munmap(vc->buffers[i].start, vc->buffers[i].length);
	}
	if (vc->fd >= 0)
		close(vc->fd);
	vc->fd = -1;
}
