#include <stdio.h>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"

// Move struct definition outside of function
struct buffer_data {
    const uint8_t *ptr;
    size_t size;
    size_t position;
};

static int read_memory(void *opaque, uint8_t *buf, int buf_size) {
    struct buffer_data *bd = (struct buffer_data *)opaque;
    
    size_t remaining = bd->size - bd->position;
    buf_size = FFMIN(buf_size, remaining);
    
    if (!buf_size)
        return AVERROR_EOF;
    
    memcpy(buf, bd->ptr + bd->position, buf_size);
    bd->position += buf_size;
    
    return buf_size;
}

int stream_to_hls(const unsigned char* buffer, size_t buffer_size) {
    AVFormatContext *input_ctx = NULL;
    AVFormatContext *output_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    uint8_t *avio_buffer = NULL;
    int ret = 0;
    struct buffer_data bd = { .ptr = buffer, .size = buffer_size, .position = 0 };
    
    // Allocate the AVIOContext buffer
    avio_buffer = av_malloc(4096);
    if (!avio_buffer) {
        fprintf(stderr, "Failed to allocate avio buffer\n");
        return -1;
    }
    
    // Create custom AVIO context
    avio_ctx = avio_alloc_context(avio_buffer, 4096, 0, &bd,
                                 read_memory, NULL, NULL);
    if (!avio_ctx) {
        av_free(avio_buffer);
        return -1;
    }
    
    // Allocate format context
    input_ctx = avformat_alloc_context();
    input_ctx->pb = avio_ctx;
    
    // Set large probe size
    AVDictionary *opts = NULL;
    av_dict_set_int(&opts, "probesize", 100 * 1024 * 1024, 0);
    av_dict_set_int(&opts, "analyzeduration", 20 * AV_TIME_BASE, 0);
    
    // Open input
    if ((ret = avformat_open_input(&input_ctx, NULL, NULL, &opts)) < 0) {
        fprintf(stderr, "Failed to open input\n");
        goto cleanup;
    }
    
    // Find stream info
    ret = avformat_find_stream_info(input_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream info\n");
        goto cleanup;
    }

    // Create output context for HLS
    ret = avformat_alloc_output_context2(&output_ctx, NULL, "hls", "outputs/output.m3u8");
    if (ret < 0) {
        fprintf(stderr, "Could not create output context\n");
        goto cleanup;
    }

    // Copy streams
    for (int i = 0; i < input_ctx->nb_streams; i++) {
        AVStream *in_stream = input_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(output_ctx, NULL);
        
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec params\n");
            goto cleanup;
        }
    }

    // Set HLS options
    AVDictionary *options = NULL;
    av_dict_set(&options, "hls_time", "4", 0);
    av_dict_set(&options, "hls_list_size", "0", 0);
    av_dict_set(&options, "hls_segment_filename", "outputs/segment%03d.ts", 0);
    av_dict_set(&options, "hls_flags", "delete_segments+independent_segments", 0);

    // Open output file
    ret = avio_open(&output_ctx->pb, "outputs/output.m3u8", AVIO_FLAG_WRITE);
    if (ret < 0) {
        fprintf(stderr, "Could not open output file\n");
        goto cleanup;
    }

    // Write header
    ret = avformat_write_header(output_ctx, &options);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when writing header\n");
        goto cleanup;
    }

    // Copy packets
    AVPacket packet;
    while (1) {
        ret = av_read_frame(input_ctx, &packet);
        if (ret < 0)
            break;

        ret = av_interleaved_write_frame(output_ctx, &packet);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }
        av_packet_unref(&packet);
    }

    av_write_trailer(output_ctx);

cleanup:
    if (input_ctx)
        avformat_close_input(&input_ctx);
    if (output_ctx) {
        avio_closep(&output_ctx->pb);
        avformat_free_context(output_ctx);
    }
    if (avio_ctx) {
        av_freep(&avio_ctx->buffer);
        av_freep(&avio_ctx);
    }
    
    return (ret < 0) ? -1 : 0;
}

