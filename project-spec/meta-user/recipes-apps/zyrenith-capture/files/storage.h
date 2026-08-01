#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

/* Creates dir (and parents) if it doesn't already exist. Returns 0 on
 * success or if the directory already exists. */
int storage_ensure_dir(const char *dir);

/* Writes a single captured frame to "<dir>/frame_<NNNN>.jpg" and
 * fsyncs it before returning, so the caller can be sure the data has
 * actually reached the storage device (not just the page cache).
 * Returns 0 on success. */
int storage_save_frame(const char *dir, unsigned int frame_number,
			const void *data, size_t length);

#endif /* STORAGE_H */
