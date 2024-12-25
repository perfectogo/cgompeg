#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/timestamp.h"
#include "libavutil/error.h"
 
#ifdef _WIN32
    #include <direct.h>  // For _mkdir on Windows
#endif

static int custom_read(void *opaque, uint8_t *buf, int buf_size) {
    FILE *file = (FILE*)opaque;
    
    size_t bytes_read = fread(buf, 1, buf_size, file);
    if (bytes_read == 0) {
        if (feof(file)) {
            printf("custom_read: EOF reached\n");
            return AVERROR_EOF;
        } else {
            perror("custom_read: Error reading file");
            return AVERROR(errno);
        }
    }
    return bytes_read;
}

/* 
this function opens the input file and returns the context
it also finds the stream info and returns the context
if the input file is not found, it returns NULL
in here we are using the avformat_open_input function to open the input file
and the avformat_find_stream_info function to find the stream info

in here function avformat_open_input and avformat_find_stream_info are in from libavformat/avformat.h
*/
AVFormatContext* open_input_file(const char *input_file) {
    
    AVFormatContext *input_ctx = NULL;
    
    int result = avformat_open_input(&input_ctx, input_file, NULL, NULL);
    {
        if (result < 0) {
            fprintf(stderr, "Error: Could not open input file '%s'.\n", input_file);
            return NULL;
        }
    }
    
    
    result = avformat_find_stream_info(input_ctx, NULL);
    {
        if (result < 0) {
            fprintf(stderr, "Error: Could not find stream info in input file.\n");
            avformat_close_input(&input_ctx);
            return NULL;
        }
    }
    
    return input_ctx;
}

/*
this function sets up the output file for hls
it also copies the streams from the input to the output
it also sets the hls options
*/
AVFormatContext* setup_hls_output(const char *output_file, AVFormatContext *input_ctx) {
    char output_dir[] = "outputs";
    char m3u8_path[1024];
    char segment_path[1024];
    
    // Create output directory
    #ifdef _WIN32
        _mkdir(output_dir);
    #else
        mkdir(output_dir, 0777);
    #endif

    // Setup paths
    snprintf(m3u8_path, sizeof(m3u8_path), "%s/%s", output_dir, output_file);
    snprintf(segment_path, sizeof(segment_path), "%s/segment%%03d.ts", output_dir);

    // Allocate output context
    AVFormatContext *output_ctx = NULL;
    int ret = avformat_alloc_output_context2(&output_ctx, NULL, "hls", m3u8_path);
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
        out_stream->codecpar->codec_tag = 0;
    }

    // Set HLS options
    AVDictionary *options = NULL;
    av_dict_set(&options, "hls_time", "10", 0);  // Increased segment duration
    av_dict_set(&options, "hls_list_size", "0", 0);
    av_dict_set(&options, "hls_segment_filename", segment_path, 0);

    // Open output file
    ret = avio_open(&output_ctx->pb, m3u8_path, AVIO_FLAG_WRITE);
    if (ret < 0) {
        fprintf(stderr, "Could not open output file '%s'\n", m3u8_path);
        avformat_free_context(output_ctx);
        return NULL;
    }

    // Write header
    ret = avformat_write_header(output_ctx, &options);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        avio_closep(&output_ctx->pb);
        avformat_free_context(output_ctx);
        return NULL;
    }

    av_dict_free(&options);
    return output_ctx;
}

/*
this function copies the packets from the input to the output
it also sets the pts, dts, duration, and pos for the output packet
it also unreferences the input packet
*/
int copy_packets(AVFormatContext *input_ctx, AVFormatContext *output_ctx) {
    int ret = 0;
    AVPacket *pkt = av_packet_alloc();  // Allocate packet
    if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        return AVERROR(ENOMEM);
    }

    // Read all packets
    while (1) {
        ret = av_read_frame(input_ctx, pkt);
        if (ret < 0) {
            break; // End of stream or error
        }

        AVStream *in_stream = input_ctx->streams[pkt->stream_index];
        AVStream *out_stream = output_ctx->streams[pkt->stream_index];

        // Copy packet
        av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;

        ret = av_interleaved_write_frame(output_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error writing packet: %s\n", av_err2str(ret));
            break;
        }

        av_packet_unref(pkt);
    }

    // Write trailer
    ret = av_write_trailer(output_ctx);
    if (ret < 0) {
        fprintf(stderr, "Error writing trailer: %s\n", av_err2str(ret));
    }

    av_packet_free(&pkt);  // Free the packet
    return ret;
}

AVFormatContext* open_input_stream(FILE* input_file) {
    AVFormatContext *input_ctx = NULL;
    unsigned char *buffer = NULL;
    AVIOContext *avio_ctx = NULL;
    size_t buffer_size = 32768;  // Increased buffer size

    // Allocate buffer
    buffer = av_malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Error: Could not allocate buffer.\n");
        return NULL;
    }

    // Create custom AVIOContext
    avio_ctx = avio_alloc_context(
        buffer,
        buffer_size,
        0,
        input_file,
        &custom_read,
        NULL,
        NULL
    );
    
    if (!avio_ctx) {
        fprintf(stderr, "Error: Could not allocate AVIOContext.\n");
        av_free(buffer);
        return NULL;
    }

    // Allocate format context
    input_ctx = avformat_alloc_context();
    if (!input_ctx) {
        fprintf(stderr, "Error: Could not allocate AVFormatContext.\n");
        if (avio_ctx) {
            av_free(avio_ctx->buffer);
            av_free(avio_ctx);
        }
        return NULL;
    }

    input_ctx->pb = avio_ctx;

    // Significantly increase probe size and analyze duration
    input_ctx->probesize = 500 * 1024 * 1024;    // 500MB probe size
    input_ctx->max_analyze_duration = 10 * AV_TIME_BASE;  // 10 seconds

    // Add format options

    AVDictionary *options = NULL;
    av_dict_set(&options, "analyzeduration", "50000000", 0); // 50 seconds
    av_dict_set(&options, "probesize", "104857600", 0); 
    // Open input with options
    int result = avformat_open_input(&input_ctx, NULL, NULL, &options);
    if (result < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(result, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error: Could not open input stream: %s\n", errbuf);
        
        av_dict_free(&options);
        if (avio_ctx) {
            av_free(avio_ctx->buffer);
            av_free(avio_ctx);
        }
        avformat_free_context(input_ctx);
        return NULL;
    }

    // Find stream info with options
    result = avformat_find_stream_info(input_ctx, NULL);
    if (result < 0) {
        fprintf(stderr, "Error: Could not find stream info.\n");
        av_dict_free(&options);
        avformat_close_input(&input_ctx);
        if (avio_ctx) {
            av_free(avio_ctx->buffer);
            av_free(avio_ctx);
        }
        return NULL;
    }

    av_dict_free(&options);
    av_dump_format(input_ctx, 0, "input", 0);

    return input_ctx;
}

int inputStream(uint8_t *data, size_t size) {
    // Create a memory file from the data
    FILE *memory_file = fmemopen(data, size, "rb");
    if (!memory_file) {
        fprintf(stderr, "Error: Could not create memory file.\n");
        return -1;
    }

    // Open the input stream
    AVFormatContext *input_ctx = open_input_stream(memory_file);
    if (!input_ctx) {
        fclose(memory_file);
        fprintf(stderr, "Error: Could not open input stream.\n");
        return -1;
    }

    // Setup HLS output
    AVFormatContext *output_ctx = setup_hls_output("output.m3u8", input_ctx);
    if (!output_ctx) {
        fclose(memory_file);
        avformat_close_input(&input_ctx);
        fprintf(stderr, "Error: Could not setup HLS output.\n");
        return -1;
    }

    // Copy packets
    int result = copy_packets(input_ctx, output_ctx);

    // Cleanup
    fclose(memory_file);
    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);

    return result;
}
