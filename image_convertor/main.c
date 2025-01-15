#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <stdio.h>

//#define AV_ERROR_EXIT(ret) if (ret < 0) { fprintf(stderr, "Error: %s\n", av_err2str(ret)); exit(1); }

void process_image(const char *input_file, const char *output_file, int width, int height, int quality, int d, char i) {
    
    AVCodecContext *decoder_ctx = NULL, *encoder_ctx = NULL;
    const AVCodec *decoder = NULL, *encoder = NULL;
    AVFrame *frame = NULL;
    struct SwsContext *sws_ctx = NULL;
    
    AVPacket *pkt = av_packet_alloc();
    if ( pkt == NULL) {
        fprintf(stderr, "Could not allocate packet\n");
        exit(1);
    }

    printf("Opening input file: %s\n", input_file);
    AVFormatContext *input_ctx = NULL;
    {
        int ret = avformat_open_input(&input_ctx, input_file, NULL, NULL);
        {
            if (ret < 0) {
                fprintf(stderr, "Could not open input file\n");
            }
        }

        printf("Finding stream info\n");
        ret = avformat_find_stream_info(input_ctx, NULL);
        {
            if (ret < 0) {
                fprintf(stderr, "Could not open input file\n");
            }
        }
    }

    int video_stream_index = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    {    
        if (video_stream_index < 0) {
            fprintf(stderr, "Could not find video stream\n");
            goto cleanup;
        }
    }
    
    AVStream *video_stream = input_ctx->streams[video_stream_index]; 
    {
        printf("Finding decoder\n");
        decoder = avcodec_find_decoder(video_stream->codecpar->codec_id);
        { 
            if (!decoder) {
                fprintf(stderr, "Could not find video decoder\n");
                goto cleanup;
            }

            decoder_ctx = avcodec_alloc_context3(decoder);
            {
                int ret = avcodec_parameters_to_context(decoder_ctx, video_stream->codecpar);
                {
                    if (ret < 0) {
                        fprintf(stderr, "Could not copy codec parameters to decoder context\n");
                        goto cleanup;
                    }
                }

                ret = avcodec_open2(decoder_ctx, decoder, NULL);
                {
                    if (ret != 0) {
                        fprintf(stderr, "Could not open codec\n");
                        goto cleanup;
                    }
                }
            }

            //printf("Input resolution: %dx%d\n", decoder_ctx->width, decoder_ctx->height);
            if (i == '<') {
                width = decoder_ctx->width / d;
                height = decoder_ctx->height / d;
            } else if (i == '>') {
                width = decoder_ctx->width * d;
                height = decoder_ctx->height * d;
            }
        }
    }

    printf("Finding encoder\n");
    encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    {    
        if (!encoder) {
            fprintf(stderr, "Could not find MJPEG encoder\n");
            goto cleanup;
        }

        encoder_ctx = avcodec_alloc_context3(encoder);
        {
            encoder_ctx->width = width;
            encoder_ctx->height = height;
            // encoder_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P; // Changed to YUVJ420P for MJPEG compatibility
            encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
            encoder_ctx->global_quality = quality;
            encoder_ctx->time_base = (AVRational){1, 1};
            encoder_ctx->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL; // Allow non-full-range YUV
            
            int ret = avcodec_open2(encoder_ctx, encoder, NULL);
            {
                if (ret < 0) {
                    fprintf(stderr, "Could not open encoder\n");
                    goto cleanup;
                }
            }
        }
    }
    
    // Allocate frame
    printf("Allocating frame\n");
    frame = av_frame_alloc();
    {
        if (frame == NULL) {
            fprintf(stderr, "Could not allocate frame\n");
            goto cleanup;
        }

        frame->color_range = AVCOL_RANGE_JPEG;
    }

    // Read, decode, scale, encode, and write frames
    while (av_read_frame(input_ctx, pkt) >= 0) {
        
        int ret = avcodec_send_packet(decoder_ctx, pkt);
        {
            if (ret != 0) {
                fprintf(stderr, "Error sending packet to decoder\n");
                goto cleanup;
            }
        }

        av_packet_unref(pkt); 

        while (avcodec_receive_frame(decoder_ctx, frame) >= 0) {
            
            printf("Decoding frame: %d x %d\n", frame->width, frame->height);

            sws_ctx = sws_getContext(frame->width, frame->height, decoder_ctx->pix_fmt, width, height, AV_PIX_FMT_YUVJ420P, SWS_BICUBIC, NULL, NULL, NULL); // Ensure the context is valid
            { 
                if (!sws_ctx) {
                    fprintf(stderr, "Could not create scaling context\n");
                    goto cleanup;
                }
}

            AVFrame *out_frame = av_frame_alloc();
            {
                if (out_frame == NULL) {
                    fprintf(stderr, "Could not allocate output frame\n");
                    goto cleanup;
                }
                
                
                out_frame->width = width;
                out_frame->height = height;
                out_frame->format = AV_PIX_FMT_YUVJ420P; // Changed to YUVJ420P for MJPEG compatibility
                int ret = av_frame_get_buffer(out_frame, 0); {
                    if (ret < 0) {
                        fprintf(stderr, "Could not allocate output frame buffer\n");
                        goto cleanup;
                    }
                }
            }

            sws_scale(sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, frame->height, out_frame->data, out_frame->linesize);

            AVPacket *encoded_pkt = av_packet_alloc();
            {
                if (encoded_pkt == NULL) {
                    fprintf(stderr, "Could not allocate packet\n");
                    goto cleanup;
                } 

                int ret = avcodec_send_frame(encoder_ctx, out_frame);
                {
                    if (ret < 0) {
                        fprintf(stderr, "Error sending frame to encoder\n");
                        goto cleanup;
                    }
                }
            } 
            

            while (avcodec_receive_packet(encoder_ctx, encoded_pkt) >= 0) {
                
                FILE *output_file_ptr = fopen(output_file, "wb");
                { 
                    if (!output_file_ptr) {
                        fprintf(stderr, "Could not open output file\n");
                        goto cleanup;
                    }
                }
                
                fwrite(encoded_pkt->data, 1, encoded_pkt->size, output_file_ptr);
                fclose(output_file_ptr);
                av_packet_unref(encoded_pkt); 
            }
            
            av_packet_free(&encoded_pkt); 
            av_frame_free(&out_frame);
            sws_freeContext(sws_ctx);
        }
    }

cleanup:
    
    if (frame) {
        av_frame_free(&frame);
    }

    if (decoder_ctx) {
        avcodec_free_context(&decoder_ctx);
    }

    if (encoder_ctx) {
        avcodec_free_context(&encoder_ctx);
    }

    if (input_ctx) {
        avformat_close_input(&input_ctx);
    }

    av_packet_free(&pkt); 
}

int main() {

    av_log_set_level(AV_LOG_DEBUG);

    // Process the image at different sizes and qualities
    process_image("input.png", "output_2.jpg", 828, 177, 10, 2, '<');
    process_image("input.png", "output_3.jpg", 800, 800, 10, 3, '<');
    process_image("input.png", "output_4.jpg", 828, 177, 10, 4, '<');
    process_image("input.png", "output_5.jpg", 800, 800, 10, 5, '<');

    return 0;
}
