#ifndef CGOMPEG_H
#define CGOMPEG_H

#include <stdint.h>

// Define the struct first
typedef struct {
    int64_t FileSize;
    char MimeType[128];
    char Extension[16];
    char Resolution[32];
} MetaData;

// Then declare the function
int read_pipe(int fd, MetaData *metadata);

#endif