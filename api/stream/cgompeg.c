#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // For read function
#include <sys/stat.h>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/timestamp.h"
#include "libavutil/error.h"

struct MetaData {
    int64_t fileSize;
    double duration;
    double bitrate;
    int32_t width;
    int32_t height;
    double frameRate;
    char mimeType[128];
    char extension[16];
    char resolution[32];
};

// Function to read metadata from the pipe
int read_metadata_from_pipe(int fd, struct MetaData *meta) {
    ssize_t bytesRead = read(fd, meta, sizeof(*meta));
    if (bytesRead != sizeof(*meta)) {
        fprintf(stderr, "Failed to read metadata from pipe (read %zd bytes, expected %zd bytes)\n", bytesRead, sizeof(*meta));
        return -1;
    }

    printf("Metadata received:\n");
    printf("File size: %ld, MimeType: %s\n", meta->fileSize, meta->mimeType);
    printf("Extension: %s, Resolution: %s\n", meta->extension, meta->resolution);
    printf("Duration: %f, Bitrate: %f, FrameRate: %f\n", meta->duration, meta->bitrate, meta->frameRate);
    printf("Width: %d, Height: %d\n", meta->width, meta->height);
    return 0;
}

// Custom read function for libav to read media data from the pipe
static int custom_read(void *opaque, uint8_t *buf, int buf_size) {
    int fd = *(int *)opaque;
    ssize_t bytesRead = read(fd, buf, buf_size);

    if (bytesRead == 0) {
        return AVERROR_EOF; // End of file
    } else if (bytesRead < 0) {
        return AVERROR(errno); // Read error
    }
    return bytesRead;
}

// Adjust probe size and analyze duration
AVFormatContext* open_input_pipe(int fd) {
    unsigned char *buffer = av_malloc(4096);
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate buffer.\n");
        return NULL;
    }

    AVIOContext *avio_ctx = avio_alloc_context(buffer, 4096, 0, &fd, custom_read, NULL, NULL);
    if (!avio_ctx) {
        fprintf(stderr, "Error: Failed to allocate AVIOContext.\n");
        av_free(buffer);
        return NULL;
    }

    AVFormatContext *input_ctx = avformat_alloc_context();
    input_ctx->pb = avio_ctx;

    // Set larger probe size and analyze duration
    AVDictionary *options = NULL;
    av_dict_set(&options, "probesize", "5000000", 0);
    av_dict_set(&options, "analyzeduration", "10000000", 0);

    if (avformat_open_input(&input_ctx, NULL, NULL, &options) < 0) {
        fprintf(stderr, "Error: Could not open input stream.\n");
        avio_context_free(&avio_ctx);
        av_dict_free(&options);
        return NULL;
    }

    av_dict_free(&options);

    if (avformat_find_stream_info(input_ctx, NULL) < 0) {
        fprintf(stderr, "Error: Could not find stream info in input stream.\n");
        avformat_close_input(&input_ctx);
        avio_context_free(&avio_ctx);
        return NULL;
    }

    return input_ctx;
}


AVFormatContext* setup_hls_output(const char *output_file, AVFormatContext *input_ctx) {
    AVFormatContext *output_ctx = NULL;
    int ret;

    // Create output directory
    char output_dir[] = "./outputs";
    #ifdef _WIN32
        _mkdir(output_dir);
    #else
        mkdir(output_dir, 0777);
    #endif

    char m3u8_path[1024];
    snprintf(m3u8_path, sizeof(m3u8_path), "%s/%s", output_dir, output_file);

    ret = avformat_alloc_output_context2(&output_ctx, NULL, "hls", m3u8_path);
    if (ret < 0) {
        fprintf(stderr, "Could not create output context\n");
        return NULL;
    }

    // Copy streams
    for (int i = 0; i < input_ctx->nb_streams; i++) {
        AVStream *in_stream = input_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(output_ctx, NULL);
        if (!out_stream) {
            avformat_free_context(output_ctx);
            return NULL;
        }

        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            avformat_free_context(output_ctx);
            return NULL;
        }
    }

    // Set HLS options
    AVDictionary *options = NULL;
    av_dict_set(&options, "hls_time", "4", 0);
    av_dict_set(&options, "hls_list_size", "0", 0);
    av_dict_set(&options, "hls_segment_filename", "outputs/segment%03d.ts", 0);

    ret = avio_open(&output_ctx->pb, m3u8_path, AVIO_FLAG_WRITE);
    if (ret < 0) {
        avformat_free_context(output_ctx);
        return NULL;
    }

    ret = avformat_write_header(output_ctx, &options);
    if (ret < 0) {
        avio_closep(&output_ctx->pb);
        avformat_free_context(output_ctx);
        return NULL;
    }

    av_dict_free(&options);
    return output_ctx;
}

int copy_packets(AVFormatContext *input_ctx, AVFormatContext *output_ctx) {
    AVPacket packet;
    int ret;

    while (1) {
        ret = av_read_frame(input_ctx, &packet);
        if (ret < 0)
            break;

        ret = av_interleaved_write_frame(output_ctx, &packet);
        if (ret < 0) {
            av_packet_unref(&packet);
            break;
        }
        av_packet_unref(&packet);
    }

    av_write_trailer(output_ctx);
    return (ret < 0 && ret != AVERROR_EOF) ? ret : 0;
}

// HLS conversion command using the pipe fd
int cmd_with_pipe_fd(int fd, const char *output_file) {
    struct MetaData meta;
    if (read_metadata_from_pipe(fd, &meta) < 0) {
        return 1;
    }

    AVFormatContext *input_ctx = open_input_pipe(fd);
    if (input_ctx == NULL) {
        fprintf(stderr, "Error: Could not open input pipe.\n");
        return 1;
    }

    AVFormatContext *output_ctx = setup_hls_output(output_file, input_ctx);
    if (output_ctx == NULL) {
        fprintf(stderr, "Error: Could not setup HLS output.\n");
        avformat_close_input(&input_ctx);
        return 1;
    }

    int result = copy_packets(input_ctx, output_ctx);
    if (result < 0) {
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return 1;
    }

    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);

    printf("HLS conversion completed successfully.\n");
    return 0;
}

int read_pipe(int fd) {
    const char *output_file = "output.m3u8";
    return cmd_with_pipe_fd(fd, output_file);
}
