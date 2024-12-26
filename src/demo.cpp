
extern "C"
{
#include <libavformat/avformat.h> // 用于输入输出（I/O）操作
#include <libavcodec/avcodec.h>   // 编码和解码操作
#include <libavutil/avutil.h>     // 工具和通用函数
}
#include <stdio.h>
#include <stdint.h>
#include "RTPSender.h"

// 查找 NALU 起始码的位置
static int find_start_code(const uint8_t *data, int size)
{
    if (!data || size < 3) // 检查输入
    {
        return -1;
    }

    for (int i = 0; i < size - 3; i++)
    {
        if ((data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) ||
            (i < size - 4 && data[i] == 0x00 && data[i + 1] == 0x00 &&
             data[i + 2] == 0x00 && data[i + 3] == 0x01))
        {
            return i; // 找到起始码
        }
    }
    return -1; // 未找到
}
static void remove_emulation_prevention(uint8_t *data, int *size)
{
    int offset = 0;
    for (int i = 0; i < *size - 2; i++)
    {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x03)
        {
            // 移除 0x03
            memmove(data + i + 2, data + i + 3, *size - (i + 3));
            (*size)--;
        }
    }
}

// 初始化 RTP 头部
void rtp_header_init(RTPHeader *header, uint16_t seq, uint32_t ts, uint32_t ssrc, int marker)
{
    memset(header, 0, sizeof(RTPHeader));
    header->version = RTP_VERSION;
    header->payload_type = RTP_PAYLOAD_TYPE;
    header->sequence_number = htons(seq);
    header->timestamp = htonl(ts);
    header->ssrc = htonl(ssrc);
    header->marker = marker;
}

int main(int argc, char *argv[])
{
    // if (argc < 2)
    // {
    //     printf("Usage: %s <input_file>\n", argv[0]);
    //     return -1;
    // }

    // 输入媒体url
    const char *url = "/home/work/Videos/video2_2024-12-26-16-02-41-122.265"; // argv[1];

    // 初始化网络功能
    avformat_network_init();
    RTPSender rtpSender;
    if (rtpSender.init("127.0.0.1", 8554) < 0)
    {
        printf("rtpSender init failed\n");
        return -1;
    }

    // 打开输入文件
    AVFormatContext *fmt_ctx = NULL;
    int ret = avformat_open_input(&fmt_ctx, url, NULL, NULL); // 文件上下文、url、格式、其他参数
    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, sizeof(err_buf));
        fprintf(stderr, "Error opening input: %s\n", err_buf);
        return -1;
    }

    // 查找流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Could not find stream info\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // 打印媒体信息
    av_dump_format(fmt_ctx, 0, url, 0);

    // 查找视频流
    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            fmt_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_HEVC)
        {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1)
    {
        fprintf(stderr, "Could not find HEVC video stream.\n");
        return -1;
    }

    // 获取视频流的时间基
    AVStream *video_stream = fmt_ctx->streams[video_stream_index];
    AVRational time_base = video_stream->time_base;

    // 读取数据包
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    uint16_t sequence_number = 0;
    uint32_t timestamp = 0;

    while (av_read_frame(fmt_ctx, &pkt) >= 0)
    {
        if (pkt.stream_index == video_stream_index)
        {
            // // 如果 PTS 无效，跳过这个包
            // if (pkt.pts == AV_NOPTS_VALUE)
            // {
            //     continue;
            // }
            // 将 PTS 转换为 RTP 时间戳
            int64_t rtp_pts = timestamp;

            // printf("Packet size: %d, pts=%" PRId64 " (%f seconds)\n", pkt.size, pkt.pts, pkt.pts * av_q2d(fmt_ctx->streams[video_stream_index]->time_base));
            int offset = 0;
            while (offset < pkt.size)
            {
                int start_code_pos = find_start_code(pkt.data + offset, pkt.size - offset);

                if (start_code_pos < 0)
                {
                    fprintf(stderr, "Warning: No start code found in packet of size %d\n", pkt.size);
                    break;
                }

                offset += start_code_pos + (pkt.data[offset + start_code_pos + 2] == 0x01 ? 3 : 4); // 处理 0x000001 或 0x00000001
                int next_start_code = find_start_code(pkt.data + offset, pkt.size - offset);
                int nalu_size = (next_start_code < 0) ? pkt.size - offset : next_start_code;
                printf("Found NALU: size=%d\n", nalu_size);

                uint8_t rtp_packet[RTP_HEADER_SIZE + RTP_MAX_PAYLOAD_SIZE];
                RTPHeader *rtp_header = (RTPHeader *)rtp_packet;
                // 如果 NALU 超过 RTP_MAX_PAYLOAD_SIZE，需要分片
                int remaining_size = nalu_size;
                const uint8_t *nalu_ptr = pkt.data + offset;
                while (remaining_size > 0)
                {
                    int payload_size = remaining_size > RTP_MAX_PAYLOAD_SIZE ? RTP_MAX_PAYLOAD_SIZE : remaining_size;
                    rtp_header_init(rtp_header, sequence_number++, rtp_pts, RTP_SSRC, remaining_size <= RTP_MAX_PAYLOAD_SIZE);
                    // 将 NALU 数据复制到 RTP 包
                    memcpy(rtp_packet + RTP_HEADER_SIZE, nalu_ptr, payload_size);
                    if (rtpSender.send(rtp_packet, RTP_HEADER_SIZE + payload_size) < 0)
                    {
                        fprintf(stderr, "RTP send failed\n");
                        break;
                    }

                    fprintf(stderr, "RTP send succeed: %d bytes\n", payload_size);
                    nalu_ptr += payload_size;
                    remaining_size -= payload_size;
                }

                // if (rtpSender.send(pkt.data + offset, nalu_size) < 0)
                // {
                //     fprintf(stderr, "RTP send failed\n");
                //     break;
                // }
                // fprintf(stderr, "RTP send succeed\n");
                offset += nalu_size;
            }
        }
        av_packet_unref(&pkt);
        timestamp +=( RTP_TIMESTAMP_INCREMENT);
    }

    // 清理资源
    avformat_close_input(&fmt_ctx);
    avformat_network_deinit();
    return 0;
}