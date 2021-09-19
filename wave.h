#ifndef _WAVE_H
#define _WAVE_H 1

#include <stdint.h>

int wave_filename(const char *filename);

int wave_openfile (const char *cwd, const char *filename, int *fd, uint64_t *offset, uint64_t *length);

#endif
