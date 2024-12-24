#include <stdio.h>
#include "libavformat/avformat.h"

int main(int argc, char **argv) {
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return -1;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];

    av_register_all(); 

    AVFormatContext *input_ctx = NULL;
    AVFormatContext *output_ctx = NULL;
    AVPacket pkt;

    // Open input file
    if (avformat_open_input(&input_ctx, input_file, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open input file.\n");
        return -1;
    }

    return 0;
}