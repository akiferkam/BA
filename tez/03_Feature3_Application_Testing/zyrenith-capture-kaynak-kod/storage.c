#include "storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int mkdir_if_missing(const char *path)
{
	struct stat st;

	if (stat(path, &st) == 0) {
		if (!S_ISDIR(st.st_mode)) {
			fprintf(stderr, "storage: %s exists and is not a directory\n",
				path);
			return -1;
		}
		return 0;
	}

	if (mkdir(path, 0775) == -1 && errno != EEXIST) {
		fprintf(stderr, "storage: mkdir(%s): %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

int storage_ensure_dir(const char *dir)
{
	char path[512];
	size_t len;
	size_t i;

	len = strlen(dir);
	if (len == 0 || len >= sizeof(path)) {
		fprintf(stderr, "storage: invalid path length for %s\n", dir);
		return -1;
	}
	memcpy(path, dir, len + 1);

	/* Walk the path and create each missing parent, e.g. for
	 * "/a/b/c" this creates "/a", then "/a/b", then "/a/b/c". */
	for (i = 1; i < len; i++) {
		if (path[i] != '/')
			continue;
		path[i] = '\0';
		if (mkdir_if_missing(path) != 0)
			return -1;
		path[i] = '/';
	}

	return mkdir_if_missing(path);
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
