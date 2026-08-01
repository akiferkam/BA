#include "storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int storage_ensure_dir(const char *dir)
{
	struct stat st;

	if (stat(dir, &st) == 0) {
		if (!(st.st_mode & S_IFDIR)) {
			fprintf(stderr, "storage: %s exists and is not a directory\n",
				dir);
			return -1;
		}
		return 0;
	}

	if (mkdir(dir, 0775) == -1 && errno != EEXIST) {
		fprintf(stderr, "storage: mkdir(%s): %s\n", dir, strerror(errno));
		return -1;
	}
	return 0;
}

int storage_save_frame(const char *dir, unsigned int frame_number,
			const void *data, size_t length)
{
	char path[512];
	int fd;
	ssize_t written;
	size_t total = 0;
	const char *buf = data;

	snprintf(path, sizeof(path), "%s/frame_%04u.jpg", dir, frame_number);

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
	if (fd < 0) {
		fprintf(stderr, "storage: open(%s): %s\n", path, strerror(errno));
		return -1;
	}

	while (total < length) {
		written = write(fd, buf + total, length - total);
		if (written < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "storage: write(%s): %s\n", path,
				strerror(errno));
			close(fd);
			return -1;
		}
		total += (size_t)written;
	}

	if (fsync(fd) == -1) {
		fprintf(stderr, "storage: fsync(%s): %s\n", path, strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}
