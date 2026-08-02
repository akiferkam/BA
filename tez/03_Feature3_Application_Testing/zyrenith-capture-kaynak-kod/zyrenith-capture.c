/*
 * zyrenith-capture - minimal capture-to-storage application skeleton.
 *
 * Ties together two independently testable components:
 *   - video_capture: V4L2 mmap-streaming capture (video_capture.c/.h)
 *   - storage:       writes frames to a mounted filesystem (storage.c/.h)
 *
 * Usage:
 *   zyrenith-capture [-d device] [-o outdir] [-w width] [-h height]
 *                     [-f fps] [-n count]
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "video_capture.h"
#include "storage.h"

static double timespec_diff_s(struct timespec start, struct timespec end)
{
	return (end.tv_sec - start.tv_sec) +
	       (end.tv_nsec - start.tv_nsec) / 1e9;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [-d device] [-o outdir] [-w width] [-h height] [-f fps] [-n count]\n"
		"  -d device   V4L2 device node (default /dev/video0)\n"
		"  -o outdir   output directory for saved frames (default /run/media/mmcblk0)\n"
		"  -w width    frame width (default 1280)\n"
		"  -h height   frame height (default 800)\n"
		"  -f fps      requested capture frame rate (default 30)\n"
		"  -n count    number of frames to capture (default 10)\n",
		prog);
}

int main(int argc, char **argv)
{
	const char *device = "/dev/video0";
	const char *outdir = "/run/media/mmcblk0";
	unsigned int width = 1280;
	unsigned int height = 800;
	unsigned int fps = 30;
	unsigned int count = 10;
	video_capture_t vc;
	struct timespec t_start, t_end;
	unsigned int captured = 0, failed = 0;
	unsigned int i;
	int opt;

	while ((opt = getopt(argc, argv, "d:o:w:h:f:n:H")) != -1) {
		switch (opt) {
		case 'd': device = optarg; break;
		case 'o': outdir = optarg; break;
		case 'w': width = (unsigned int)atoi(optarg); break;
		case 'h': height = (unsigned int)atoi(optarg); break;
		case 'f': fps = (unsigned int)atoi(optarg); break;
		case 'n': count = (unsigned int)atoi(optarg); break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	printf("zyrenith-capture: device=%s outdir=%s %ux%u @%ufps, count=%u\n",
	       device, outdir, width, height, fps, count);

	if (storage_ensure_dir(outdir) != 0)
		return 1;

	if (video_capture_open(&vc, device, width, height, fps) != 0) {
		fprintf(stderr, "zyrenith-capture: failed to open %s\n", device);
		return 1;
	}
	printf("zyrenith-capture: negotiated format %ux%u, %u mmap buffers\n",
	       vc.width, vc.height, vc.buffer_count);

	if (video_capture_start(&vc) != 0) {
		video_capture_close(&vc);
		return 1;
	}

	clock_gettime(CLOCK_MONOTONIC, &t_start);

	for (i = 0; i < count; i++) {
		void *data;
		size_t length;
		unsigned int buf_index;

		if (video_capture_grab_frame(&vc, &data, &length, &buf_index) != 0) {
			fprintf(stderr, "zyrenith-capture: grab failed on frame %u\n", i);
			failed++;
			continue;
		}

		if (storage_save_frame(outdir, i, data, length) != 0) {
			fprintf(stderr, "zyrenith-capture: save failed on frame %u\n", i);
			failed++;
		} else {
			captured++;
		}

		if (video_capture_release_frame(&vc, buf_index) != 0) {
			fprintf(stderr, "zyrenith-capture: release failed on frame %u\n", i);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &t_end);

	video_capture_stop(&vc);
	video_capture_close(&vc);

	{
		double elapsed = timespec_diff_s(t_start, t_end);
		double achieved_fps = elapsed > 0 ? captured / elapsed : 0.0;

		printf("zyrenith-capture: done. captured=%u failed=%u elapsed=%.3fs achieved=%.2ffps\n",
		       captured, failed, elapsed, achieved_fps);
	}

	return failed > 0 ? 1 : 0;
}
