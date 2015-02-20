#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <openssl/md5.h>
#include "defenitions.h"
#include "messages.h"

#define DEFAULT_PORT 7777
#define DEFAULT_HOST (char*)"localhost"
#define BUFFER_SERVER_SIZE 0x400 // = 1024

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define bswap64(y) (((uint64_t)ntohl(y)) << 32 | ntohl(y>>32))
#else
#define bswap64(y) (y)
#endif

SOCKET sock;

int sendMessage(short msgCode, char* data, int datalen);
short getMsgCode(char* data, unsigned int datalen);
void uploadFile(FILE* file);
void downloadFile(FILE* file, uint32_t blockCount);

int main(int argc, char* argv[])
{
    srand(time(NULL));
    char Buffer[BUFFER_SERVER_SIZE + 150];
    char* server_name = DEFAULT_HOST; // default to localhost
    if(argc > 1)
        server_name = argv[1];
    unsigned short port = DEFAULT_PORT;
    if(argc > 2)
        port = (atoi(argv[2]) & 0xFFFF);
    int retval;
    struct sockaddr_in server;
    struct hostent *hp;
    short msgCode;
    int len;
    FILE* fileTemp = NULL;

    if (argc > 1)
        server_name = argv[1];

    union {
        char str[0xFF];
        uint64_t l;
        int i;
    } tempdata;

#ifdef WIN32
    WSADATA wsaData;
    if ((retval = WSAStartup(0x202, &wsaData)) != 0)
    {
        fprintf(stderr,"Server: WSAStartup() failed with error %d\n", retval);
        goto _badExit;
    }
#endif

    memset(&server, 0, sizeof(server));
    if (isalpha(server_name[0]))
    {   // server address is a name
        hp = gethostbyname(server_name);
        if (hp == NULL )
        {
#ifdef WIN32
            fprintf(stderr,"Client: Cannot resolve address \"%s\": Error %d\n", server_name, WSAGetLastError());
#else
            fprintf(stderr,"Client: Cannot resolve address \"%s\"\n", server_name);
#endif
            goto _badExit;
        }
        memcpy(&(server.sin_addr), hp->h_addr, hp->h_length);
    }
    else
    {   // Convert IP address to a usable one
#ifdef WIN32	
        server.sin_addr.S_un.S_addr = inet_addr(server_name);
#else
        inet_aton(server_name, &server.sin_addr);
#endif
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock <0 )
    {
#ifdef WIN32
        fprintf(stderr,"Client: Error Opening socket: Error %d\n", WSAGetLastError());
#else
        fprintf(stderr,"Client: Error Opening socket\n");
#endif
        goto _badExit;
    }

    printf("Client: Client connecting to: %s\n", server_name);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
    {
#ifdef WIN32
        fprintf(stderr,"Client: connect() failed: %d\n", WSAGetLastError());
#else
        fprintf(stderr,"Client: connect() failed\n");
#endif
        goto _badExit;
    }
    else
        printf("Client: connect() is OK.\n");

    while(true)
    {
        printf("enter msg code in dec mode:");
        scanf("%hd", &msgCode);
        switch (msgCode)
        {
        case CLIENT_MSG::SERVER_INFO:
        case CLIENT_MSG::CLIENT_INFO:
        case CLIENT_MSG::LOGOUT:
        case CLIENT_MSG::DIR_PWD:
            len = 0;
            break;
        case CLIENT_MSG::LOGIN:
            printf("username: ");
            scanf("%s", Buffer + 16);
            printf("password: ");
            scanf("%s", tempdata.str);
            MD5((uint8_t*)tempdata.str, strlen(tempdata.str), (uint8_t*)Buffer);
            len = strlen(Buffer + 16) + 16;
            break;
        case CLIENT_MSG::FILE_UPLOAD:
            if(fileTemp)
                fclose(fileTemp);
            while(!fileTemp)
            {
                printf("enter path to local file: ");
                scanf("%s", tempdata.str);
                fileTemp = fopen(tempdata.str, "rb");
            }
            fseek(fileTemp, 0, SEEK_END);
            tempdata.l = ftell(fileTemp);
            fseek(fileTemp, 0, SEEK_SET);
            {
                uint32_t count = (uint32_t)(tempdata.l / 0x200);
                if(count * 0x200 < tempdata.l)
                    ++count;
                *(uint32_t*)(Buffer) = htonl(count);
            }
            printf("enter remote path: ");
            scanf("%s", Buffer + 4);
            break;
        case CLIENT_MSG::FILE_DOWNLOAD:
            printf("enter remote path: ");
            scanf("%s", Buffer);
            if(fileTemp)
            {
                fclose(fileTemp);
                fileTemp = NULL;
            }
            while(!fileTemp)
            {
                printf("enter path to local file: ");
                scanf("%s", tempdata.str);
                fileTemp = fopen(tempdata.str, "wb");
            }
            break;
        case CLIENT_MSG::FILE_MOVE:
        case CLIENT_MSG::FILE_COPY:
        case CLIENT_MSG::DIR_MOVE:
        case CLIENT_MSG::DIR_COPY:
        case CLIENT_MSG::FILE_SYMLINK:
            printf("enter source relative path: ");
            scanf("%s", Buffer + 1); // src
            *Buffer = strlen(Buffer + 1) & 0xFF; // src_len
            *(Buffer + 2 + *Buffer) = 0; // padding of zero
            printf("enter destination relative path: ");
            scanf("%s", Buffer + 4 + *Buffer); // dst
            *(Buffer + 3 + *Buffer) = strlen(Buffer + 4 + *Buffer) & 0xFF; // dst_len
            len = 5 + *Buffer + *(Buffer + 3 + *Buffer);
            break;
        default:
            printf("enter data:");
            scanf("%s", Buffer);
            len = strlen(Buffer);
            break;
        }
        if (sendMessage(msgCode, Buffer, len) == SOCKET_ERROR)
        {
            goto _badSend;
        }
        if ((retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == SOCKET_ERROR)
        {
            goto _badRecv;
        }
        Buffer[retval] = 0;
        tempdata.i = getMsgCode(Buffer, retval);
        if(msgCode == CLIENT_MSG::FILE_DOWNLOAD || msgCode == CLIENT_MSG::FILE_UPLOAD)
        {
            if(tempdata.i == SERVER_MSG::ACTION_COMPLETED)
            {
                if(msgCode == CLIENT_MSG::FILE_UPLOAD)
                    uploadFile(fileTemp);
                else
                    downloadFile(fileTemp, ntohl(*((int*)(Buffer + 2))));
                retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0);
                tempdata.i = getMsgCode(Buffer, retval);
            }
            fclose(fileTemp);
            fileTemp = NULL;
        }
        else if(msgCode == CLIENT_MSG::LOGOUT)
        {
            break;
        }
        else if(msgCode == CLIENT_MSG::FILE_MD5 && tempdata.i == SERVER_MSG::ACTION_COMPLETED)
        {
            printf("got this hash: ");
            for(tempdata.i = 2; tempdata.i < 18; tempdata.i++)
                printf("%02X", ((uint8_t*)Buffer)[tempdata.i]);
            printf("\n");
            if ((retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == SOCKET_ERROR)
                goto _badRecv;
            continue;
        }
        else if(msgCode == CLIENT_MSG::FILE_SIZE && tempdata.i == SERVER_MSG::ACTION_COMPLETED)
        {
            tempdata.l = bswap64(*((uint64_t*)(Buffer + 2)));
#ifdef WIN32
            printf("File size is %I64u\n", tempdata.l);
#else
            printf("File size is %llu\n", tempdata.l);
#endif
            if ((retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == SOCKET_ERROR)
                goto _badRecv;
            continue;
        }
        else if(tempdata.i == SERVER_MSG::TIMEOUT)
        {
            if ((retval = sendMessage(CLIENT_MSG::EMPTY_MESSAGE, NULL, 0)) == SOCKET_ERROR)
            {
                goto _badSend;
            }
            if ((retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == SOCKET_ERROR)
                goto _badRecv;
            continue;
        }
        while(tempdata.i == SERVER_MSG::LS_DATA)
        {
            printf("%s", Buffer + sizeof(msgCode));
            if ((retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == SOCKET_ERROR)
            {
                goto _badRecv;
            }
            tempdata.i = getMsgCode(Buffer, retval);
        }
        Buffer[retval] = 0;
        printf("\ngot this code: %5d, data: %s\n", tempdata.i, Buffer + sizeof(msgCode));
    }
    closesocket(sock);
#ifdef WIN32
    WSACleanup();
#endif
    return (0);

_badRecv:
    fprintf(stderr,"Client: recv() failed.\n");
    closesocket(sock);
    goto _badExit;
_badSend:
    fprintf(stderr,"Client: send() failed.\n");
    goto _badExit;
_badExit:
#ifdef WIN32
    WSACleanup();
#endif
    return (-1);
}

int sendMessage(short msgCode, char* data, int datalen)
{
    static bool lockSend = false; // mini mutex

    char buffer[BUFFER_SERVER_SIZE + 5];
    msgCode = htons(msgCode);
    memcpy(buffer, &msgCode, 2);
    if(datalen > BUFFER_SERVER_SIZE)
        datalen = BUFFER_SERVER_SIZE;
    if(data && datalen > 0)
        memcpy(buffer + 2, data, datalen);

    while (lockSend) ;
    lockSend = true;
    int retVal = send(sock, buffer, datalen + sizeof(msgCode), 0);
    lockSend = false;
    return (retVal);
}

short getMsgCode(char* data, unsigned int datalen)
{
    short ret;
    if(datalen < sizeof(short))
        return (-1);
    memcpy(&ret, data, sizeof(ret));
    ret = ntohs(ret);
    return (ret);
}

void uploadFile(FILE* file)
{
    struct __attribute__((packed)){
        uint32_t blockNum;
        uint16_t size;
        uint8_t md5Res[16];
        uint8_t dataFile[0x200];
    } data;
    struct __attribute__((packed)){
        uint16_t msgCode;
        union __attribute__((packed)){
            char data[BUFFER_SERVER_SIZE - sizeof(msgCode)];
            uint32_t completedBlock;
        }u;
    } Buffer;
    int blocksCount, i;
    uint8_t* blocks; // 1 - bad, 0 - good
    int flag = 1;
    size_t readBytes;
    uint32_t blockNum;
    for(blockNum = 0; (readBytes = fread(data.dataFile, 1, 0x200, file)); ++blockNum)
    {
        MD5(data.dataFile, readBytes, data.md5Res);
        data.blockNum = htonl(blockNum);
        data.size = htons(readBytes);
        sendMessage(CLIENT_MSG::FILE_BLOCK, (char*)&data, 22 + readBytes);
    }
    blocksCount = blockNum;
    blocks = (uint8_t*)malloc(blocksCount);
    memset(blocks, 1, blocksCount);
    while(flag)
    {
        recv(sock, (char*)&Buffer, BUFFER_SERVER_SIZE, 0);
        if(ntohs(Buffer.msgCode) == SERVER_MSG::ACTION_COMPLETED)
        {
            blocks[ntohl(Buffer.u.completedBlock)] = 0;
            flag = 0;
            for(i = 0; !flag || i < blocksCount; i++)
                flag |= blocks[i]; // only if the whole array it 0 (we finished) than flag will be 0
        }
        else
        {
            data.blockNum = ntohl(Buffer.u.completedBlock);
            fseek(file, data.blockNum * 0x200, SEEK_SET);
            data.size = fread(data.dataFile, 1, 0x200, file);
            MD5(data.dataFile, data.size, data.md5Res);

            data.blockNum = htonl(data.blockNum);
            data.size = htons(data.size);
            sendMessage(CLIENT_MSG::FILE_BLOCK, (char*)&data, 32 + data.size);
            data.blockNum = ntohl(data.blockNum);
        }
    }
    free(blocks);
    sendMessage(CLIENT_MSG::END_FILE_TRANSFER, NULL, 0);
}

void downloadFile(FILE* file, uint32_t blockCount)
{
    struct __attribute__((packed)) {
        uint16_t msgCode;
        struct __attribute__((packed)){
            uint32_t blockNum;
            uint16_t size;
            uint8_t md5Res[MD5_DIGEST_LENGTH];
            uint8_t dataFile[0x200];
        } download;
        char emptyData[BUFFER_SERVER_SIZE - sizeof(msgCode) - sizeof(download)];
    } Buffer;
    uint32_t range[2] = {0, htonl(blockCount)};
    sendMessage(CLIENT_MSG::ASK_BLOCK_RANGE, (char*)range, sizeof(range));
    int i;
    uint8_t* blocks = (uint8_t*)malloc(blockCount); // 1 - bad, 0 - good
    uint8_t md5Res[MD5_DIGEST_LENGTH];
    int flag = 1;
    while(flag)
    {
        i = recv(sock, (char*)&Buffer, sizeof(Buffer), 0);
        Buffer.msgCode = ntohs(Buffer.msgCode);

        Buffer.download.blockNum = ntohl(Buffer.download.blockNum);
        Buffer.download.size = ntohs(Buffer.download.size);
        MD5(Buffer.download.dataFile, Buffer.download.size, md5Res);
        if(memcmp(Buffer.download.md5Res, md5Res, MD5_DIGEST_LENGTH)) // not equal
        {
            Buffer.download.blockNum = htonl(Buffer.download.blockNum);
            sendMessage(CLIENT_MSG::ASK_BLOCK, (char*)&Buffer.download.blockNum, sizeof(Buffer.download.blockNum)); // ask for block
        }
        else
        {
            fseek(file, Buffer.download.blockNum * 0x200, SEEK_SET);
            i = fwrite(Buffer.download.dataFile, 1, Buffer.download.size, file);
            blocks[Buffer.download.blockNum] = 0;
            flag = 0;
            for(i = 0; !flag && i < blockCount; i++)
                flag |= blocks[i]; // only if the whole array it 0 (we finished) than flag will be 0
        }
    }
    free(blocks);
    sendMessage(CLIENT_MSG::END_FILE_TRANSFER, NULL, 0);
}
