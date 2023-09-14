#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <librtmp/rtmp.h>

// 大小端字节序转换
#define HTON16(x) ( (x >> 8 & 0x00FF) | (x << 8 & 0xFF00) )
#define HTON24(x) ( (x >> 16 & 0x0000FF) | (x & 0x00FF00) | (x << 16 & 0xFF0000) )
#define HTON32(x) ( (x >> 24 & 0x000000FF) | (x >> 8 & 0x0000FF00) | (x << 8 & 0x00FF0000) | (x << 24 & 0xFF000000) )
#define HTONTIME(x) ( (x >> 16 & 0x000000FF) | (x & 0x0000FF00) | (x << 16 & 0x00FF0000) | (x & 0xFF000000) )

// 从文件读取指定字节
bool ReadFP(char* pBuf, int nSize, FILE* pFile)
{
    return (fread(pBuf, 1, nSize, pFile) == nSize);
}

// 从文件读取1个字节整数
bool ReadU8(uint8_t* u8, FILE* fp)
{
    return ReadFP((char*)u8, 1, fp);
}

// 从文件读取2个字节整数
bool ReadU16(uint16_t* u16, FILE* fp)
{
    if (!ReadFP((char*)u16, 2, fp))
        return false;

    *u16 = HTON16(*u16);
    return true;
}

// 从文件读取3个字节整数
bool ReadU24(uint32_t* u24, FILE* fp)
{
    if (!ReadFP((char*)u24, 3, fp))
        return false;

    *u24 = HTON24(*u24);
    return true;
}

// 从文件读取4个字节整数
bool ReadU32(uint32_t* u32, FILE* fp)
{
    if (!ReadFP((char*)u32, 4, fp))
        return false;

    *u32 = HTON32(*u32);
    return true;
}

// 从文件读取4个字节时间戳
bool ReadTime(uint32_t* utime, FILE* fp)
{
    if (!ReadFP((char*)utime, 4, fp))
        return false;

    *utime = HTONTIME(*utime);
    return true;
}

// 从文件预读1个字节整数
bool PeekU8(uint8_t* u8, FILE* fp)
{
    if (!ReadFP((char*)u8, 1, fp))
        return false;

    fseek(fp, -1, SEEK_CUR);
    return true;
}

int main(int argc, char* argv[])
{
    FILE* pFile = fopen("1.flv", "rb");

    // 初使化RTMP上下文
    RTMP* pRTMP = RTMP_Alloc();
    RTMP_Init(pRTMP);

    // 设置推流地址
    pRTMP->Link.timeout = 10;
    RTMP_SetupURL(pRTMP, (char*)"rtmp://127.0.0.1:1935/live/a");

    // 开启推流标志
    RTMP_EnableWrite(pRTMP);

    // 连接服务器
    bool b = RTMP_Connect(pRTMP, NULL);
    if (!b)
    {
        printf("connect failed! \n");
        return -1;
    }
        
    // 连接流地址
    b = RTMP_ConnectStream(pRTMP, 0);
    if (!b)
    {
        printf("connect stream failed! \n");
        return -1;
    }

    // 跳过FLV文件头的13个字节
    fseek(pFile, 9, SEEK_SET);
    fseek(pFile, 4, SEEK_CUR);

    // 初使化RTMP报文
    RTMPPacket packet;
    RTMPPacket_Reset(&packet);
    packet.m_body = NULL;
    packet.m_chunk = NULL;

    packet.m_nInfoField2 = pRTMP->m_stream_id;

    uint32_t starttime = RTMP_GetTime();

    while (true)
    {
        // 读取TAG头

        uint8_t type = 0;
        if (!ReadU8(&type, pFile))
            break;

        uint32_t datalen = 0;
        if (!ReadU24(&datalen, pFile))
            break;

        uint32_t timestamp = 0;
        if (!ReadTime(&timestamp, pFile))
            break;

        uint32_t streamid = 0;
        if (!ReadU24(&streamid, pFile))
            break;

/*
        // 跳过0x12 Script
        if (type != 0x08 && type != 0x09)
        {
            fseek(pFile, datalen + 4, SEEK_CUR);
            continue;
        }
*/

        RTMPPacket_Alloc(&packet, datalen);

        if (fread(packet.m_body, 1, datalen, pFile) != datalen)
            break;

        // 组织报文并发送
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = type;
        packet.m_hasAbsTimestamp = 0;
        packet.m_nChannel = 6;
        packet.m_nTimeStamp = timestamp;
        packet.m_nBodySize = datalen;

        if (!RTMP_SendPacket(pRTMP, &packet, 0))
        {
            printf("Send Error! \n");
            break;
        }

        printf("send type:[%d] timestamp:[%d] datasize:[%d] \n", type, timestamp, datalen);

        // 跳过PreTag
        uint32_t pretagsize = 0;
        if (!ReadU32(&pretagsize, pFile))
            break;

        // 延时，避免发送太快
        uint32_t timeago = (RTMP_GetTime() - starttime);
        if (timestamp > 1000 && timeago < timestamp - 1000)
        {
            printf("sleep...\n");
            usleep(100000);
        }

        RTMPPacket_Free(&packet);
    }

    // 关闭连接，释放RTMP上下文
    RTMP_Close(pRTMP);
    RTMP_Free(pRTMP);

    fclose(pFile);

    return 0;
}