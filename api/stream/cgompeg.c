#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#include "cgompeg.h"

#define TEMP_FILE "tmp/temp.mp4"

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
    {
        if (bytes_read == 0) {
            return AVERROR_EOF;
        }
    }

    return bytes_read;
}

// int custom_read(void *opaque, uint8_t *buf, int buf_size) {
//     FILE *file = (FILE*)opaque;
//     return fread(buf, 1, buf_size, file);
// }

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
    
    char m3u8_path[1024];
    char output_dir[] = "outputs";
    {   
        #ifdef _WIN32
            _mkdir(output_dir);
        #else
            mkdir(output_dir, 0777);
        #endif
    }

    snprintf(m3u8_path, sizeof(m3u8_path), "%s/%s", output_dir, output_file);
    AVFormatContext *output_ctx = NULL;

    /*
    this function allocates the output context
    it also sets the output format to hls
    it also sets the output file
    */
    int result = avformat_alloc_output_context2(&output_ctx, NULL, "hls", m3u8_path);
    { 
        if (result < 0) {
            fprintf(stderr, "Error: Could not create output context.\n");
            return NULL;
        }
    }

    /*
    this block copies the streams from the input to the output
    it also sets the codec parameters
    */
    for (int i = 0; i < input_ctx->nb_streams; i++) {
        
        AVStream *in_stream = input_ctx->streams[i];
        
        /*
        this function allocates a new stream
        it also sets the codec parameters as the same as the input stream. 
        Example: video, audio, etc.
        */
        AVStream *out_stream = avformat_new_stream(output_ctx, NULL); 
        {   
            if (out_stream == NULL) {
                fprintf(stderr, "Error: Failed to allocate output stream.\n");
                return NULL;
            }
        }

        /*
        this function copies the codec parameters from the input stream to the output stream
        */
        int result = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        {
            if (result < 0) {
                fprintf(stderr, "Error: Failed to copy codec parameters.\n");
                return NULL;
            }
        }

        out_stream->codecpar->codec_tag = 0;
    }

    /*
    this block sets the hls options
    hls_time: the duration of each segment in seconds
    hls_list_size: the number of segments to keep in the playlist
    hls_segment_filename: the filename format for the segments
    */
    char segment_path[1024];
    snprintf(segment_path, sizeof(segment_path), "%s/segment%%03d.ts", output_dir);
   
    AVDictionary *options = NULL;
    { 
        av_dict_set(&options, "hls_time", "1", 0); // Segment duration
        av_dict_set(&options, "hls_list_size", "0", 0); // Unlimited playlist size
        av_dict_set(&options, "hls_segment_filename", segment_path, 0);
        // av_dict_set(&options, "video_bitrate", "1000000", 0);
        // av_dict_set(&options, "audio_bitrate", "128000", 0);
        // av_dict_set(&options, "hls_flags", "delete_segments", 0);
    }
    
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {

        int result = avio_open(&output_ctx->pb, output_file, AVIO_FLAG_WRITE);
        {
            if (result < 0) {
                fprintf(stderr, "Error: Could not open output file '%s'.\n", output_file);
                return NULL;
            }
        }
    }

    /*
    this block writes the header to the output file. Ex: in xxxxx.m3u8
    it also sets the hls options. Example: hls_time, hls_list_size, hls_segment_filename
    */
    result = avformat_write_header(output_ctx, &options);
    {
        if (result < 0) {
            fprintf(stderr, "Error: Could not write header to output file.\n");
            return NULL;
        }
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
    
    AVPacket pkt;
    
    /*
    this block reads the packets from the input
    it also sets the pts, dts, duration, and pos for the output packet
    it also unreferences the input packet
    */
    while (av_read_frame(input_ctx, &pkt) >= 0) {
        
        AVStream *in_stream = input_ctx->streams[pkt.stream_index];
        AVStream *out_stream = output_ctx->streams[pkt.stream_index];
        {
            pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
            pkt.pos = -1;
        }

        int result = av_interleaved_write_frame(output_ctx, &pkt);
        {
            if (result < 0) {
                fprintf(stderr, "Error: Failed to write frame to output file.\n");
                av_packet_unref(&pkt);
                return -1;
            }
        }

        av_packet_unref(&pkt);
    }

    int result = av_write_trailer(output_ctx);
    {
        if (result < 0) {
            fprintf(stderr, "Error: Failed to write trailer to output file.\n");
            return -1;
        }
    }

    return 0;
}

int cmd(const char *input_file, const char *output_file) {
    
    av_log_set_level(AV_LOG_QUIET);

    AVFormatContext *input_ctx = open_input_file(input_file);
    { 
        if (input_ctx == NULL) { 
            fprintf(stderr, "Error: Could not open input file.\n");
            return 1;
        };
    }

    AVFormatContext *output_ctx = setup_hls_output(output_file, input_ctx);
    {
        if (output_ctx == NULL) {
            fprintf(stderr, "Error: Could not setup HLS output.\n");
            avformat_close_input(&input_ctx);
            avformat_free_context(output_ctx);
            return 1;
        }
    }

    int result = copy_packets(input_ctx, output_ctx);
    {   
        if (result < 0) {
            avformat_close_input(&input_ctx);
            avformat_free_context(output_ctx);
            return 1;
        }
    }
    
    {
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
    }

    printf("HLS conversion completed successfully.\n");
   
    return 0;
}


int read_pipe(int fd, MetaData *metadata) {
    // Create tmp directory if it doesn't exist
    #ifdef _WIN32
        _mkdir("tmp");
    #else
        mkdir("tmp", 0777);
    #endif

    // debug hls

    av_log_set_level(AV_LOG_DEBUG);

    // printf("file size: %" PRId64 "\n", metadata->FileSize);
    // printf("extension: %s\n", metadata->Extension);
    // printf("mime type: %s\n", metadata->MimeType);
    // printf("resolution: %s\n", metadata->Resolution);

    // create .mp4 file
    FILE *file = fopen(TEMP_FILE, "wb");
    if (!file) {
        perror("Error: Could not open file descriptor as a file");
        return 1;
    }

    // read from pipe
    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes_read, file);
    }

    fclose(file);  // Close file before passing to cmd

    // Process the video
    int result = cmd(TEMP_FILE, "output.m3u8");

    // Clean up temp file
    remove(TEMP_FILE);

    return result;
}

// // Define thread argument struct
// struct ThreadArgs {
//     int fd;
//     FILE *file;
// };

// // Update the thread functions and add mutex for synchronization
// pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_cond_t write_done = PTHREAD_COND_INITIALIZER;
// volatile int writing_complete = 0;

// void* writer_thread(void *arg) {
//     struct ThreadArgs *args = (struct ThreadArgs*)arg;
//     char buffer[8192];  // Increased buffer size
//     ssize_t bytes_read;

//     while ((bytes_read = read(args->fd, buffer, sizeof(buffer))) > 0) {
//         pthread_mutex_lock(&file_mutex);
//         if (fwrite(buffer, 1, bytes_read, args->file) != bytes_read) {
//             pthread_mutex_unlock(&file_mutex);
//             return (void*)(intptr_t)1;
//         }
//         fflush(args->file);
//         pthread_mutex_unlock(&file_mutex);
//     }

//     pthread_mutex_lock(&file_mutex);
//     writing_complete = 1;
//     pthread_cond_signal(&write_done);
//     pthread_mutex_unlock(&file_mutex);
    
//     return NULL;
// }

// void* stream_thread(void *arg) {
//     pthread_mutex_lock(&file_mutex);
//     while (!writing_complete) {
//         pthread_cond_wait(&write_done, &file_mutex);
//     }
//     pthread_mutex_unlock(&file_mutex);

//     // Ensure file exists and is readable
//     if (access(TEMP_FILE, R_OK) != 0) {
//         fprintf(stderr, "Error: Cannot access temporary file\n");
//         return (void*)(intptr_t)1;
//     }

//     int res = cmd(TEMP_FILE, "output.m3u8");
//     return (void*)(intptr_t)res;
// }

// int run_threads(int fd) {
//     pthread_t writer_thread_id;
//     pthread_t stream_thread_id;
//     void *thread_result;
//     int ret = 0;

//     // Reset global state
//     writing_complete = 0;

//     // Create tmp directory
//     #ifdef _WIN32
//         _mkdir("tmp");
//     #else
//         mkdir("tmp", 0777);
//     #endif

//     FILE *file = fopen(TEMP_FILE, "wb");
//     if (!file) {
//         perror("Error: Could not open temporary file");
//         return 1;
//     }

//     struct ThreadArgs args = {fd, file};

//     if (pthread_create(&writer_thread_id, NULL, writer_thread, &args) != 0) {
//         fclose(file);
//         return 1;
//     }

//     if (pthread_create(&stream_thread_id, NULL, stream_thread, NULL) != 0) {
//         pthread_cancel(writer_thread_id);
//         pthread_join(writer_thread_id, NULL);
//         fclose(file);
//         return 1;
//     }

//     // Wait for both threads
//     pthread_join(writer_thread_id, &thread_result);
//     if (thread_result) ret = 1;

//     pthread_join(stream_thread_id, &thread_result);
//     if (thread_result) ret = 1;

//     // Cleanup
//     fclose(file);
//     remove(TEMP_FILE);

//     return ret;
// }

// int read_pipe(int fd, MetaData *metadata) {
//     #ifdef _WIN32
//         _mkdir("tmp");
//     #else
//         mkdir("tmp", 0777);
//     #endif

//     return run_threads(fd);
// }