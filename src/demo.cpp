
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#include <stdio.h>
#include <stdint.h>

// 查找 NALU 起始码的位置
static int find_start_code(const uint8_t *data, int size) {
    for (int i = 0; i < size - 3; i++) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && 
            data[i + 2] == 0x01) {
            return i; // 找到起始码
        }
    }
    return -1; // 未找到
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return -1;
    }

    // 初始化 FFmpeg 网络功能（如果需要）
    avformat_network_init();

    // 打开输入文件
    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) < 0) {
        fprintf(stderr, "Could not open input file.\n");
        return -1;
    }

    // 查找流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information.\n");
        return -1;
    }

    // 查找视频流
    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            fmt_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_HEVC) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        fprintf(stderr, "Could not find HEVC video stream.\n");
        return -1;
    }

    // 读取数据包
    AVPacket pkt;
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == video_stream_index) {
            printf("Packet size: %d\n", pkt.size);
            
            // 手动解析 NALU
            int offset = 0;
            while (offset < pkt.size) {
                int start_code_pos = find_start_code(pkt.data + offset, pkt.size - offset);
                if (start_code_pos < 0) break;

                offset += start_code_pos + 3; // 跳过起始码
                int next_start_code = find_start_code(pkt.data + offset, pkt.size - offset);
                int nalu_size = (next_start_code < 0) ? 
                                pkt.size - offset : next_start_code;

                printf("Found NALU: size=%d\n", nalu_size);
                offset += nalu_size;
            }
        }
        av_packet_unref(&pkt);
    }

    // 清理资源
    avformat_close_input(&fmt_ctx);
    return 0;
}