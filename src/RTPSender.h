#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define RTP_VERSION 2
#define RTP_PAYLOAD_TYPE 0 // 96 // 动态负载类型，H.265 通常使用 96
#define RTP_HEADER_SIZE 12
#define RTP_MAX_PAYLOAD_SIZE 1400 // RTP 最大负载，通常小于 MTU 1500
#define RTP_SSRC 0x12345678
#define LOCK_RATE 90000 // 视频时钟频率
#define FRAME_RATE 30   // 视频帧率 (FPS)
#define RTP_TIMESTAMP_INCREMENT LOCK_RATE / FRAME_RATE

// RTP 头部结构
typedef struct
{
    uint8_t csrc_count : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payload_type : 7;
    uint8_t marker : 1;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
} RTPHeader;

class RTPSender
{
private:
    int sock;
    struct sockaddr_in dest_addr;

public:
    RTPSender()
    {
    }

    // 初始化 RTP Socket,ip port
    int init(const char *ip, int port)
    {
        // 检查输入合法性
        if (!ip || port <= 0 || port > 65535) // 检查输入合法性
        {
            fprintf(stderr, "Invalid IP or port\n");
            return -1;
        }

        // 创建 UDP Socket
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0)
        {
            perror("socket failed");
            return -1;
        }

        // 初始化 dest_addr
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        if (inet_addr(ip) == INADDR_NONE)
        {
            fprintf(stderr, "Invalid IP address format\n");
            return -1;
        }
        dest_addr.sin_addr.s_addr = inet_addr(ip);
        printf("码流传输目标地址 rtp://%s:%d\n", inet_ntoa(dest_addr.sin_addr), port);
        return 0;
    }

    // RTP 发送逻辑
    int send(const uint8_t *data, size_t size)
    {
        if (sendto(sock, data, size, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
        {
            perror("sendto failed");
            return -1;
        }
        return 0;
    }

    ~RTPSender()
    {
        close(sock);
    }
};
