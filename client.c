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

#define DEFAULT_PORT 7777
#define DEFAULT_HOST "localhost"
#define BUFFER_SERVER_SIZE 0x400 // = 1024

SOCKET sock;

int sendMessage(short msgCode, char* data, int datalen);
short getMsgCode(char* data, unsigned int datalen);
void uploadFile(FILE* file);
void downloadFile(FILE* file, int blockCount);

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
        case 500:
        case 105:
        case 536:
            len = 0;
            break;
        case 100:
            printf("username: ");
            scanf("%s", Buffer + 16);
            printf("password: ");
            scanf("%s", tempdata.str);
            MD5((uint8_t*)tempdata.str, strlen(tempdata.str), (uint8_t*)Buffer);
            len = strlen(Buffer + 16) + 16;
            break;
        case 510:
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
            *(int*)(Buffer) = htonl((tempdata.l / 0x200));
            if(*(int*)(Buffer) * 0x200 < tempdata.l)
                *(int*)(Buffer) = htonl(1 + *(int*)(Buffer));
            printf("enter remote path: ");
            scanf("%s", Buffer + 4);
            break;
        case 511:
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
        case 520:
        case 521:
        case 533:
        case 534:
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
            fprintf(stderr,"Client: send() failed.\n");
            goto _badExit;
        }
        if ((retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == SOCKET_ERROR)
        {
            fprintf(stderr,"Client: recv() failed.\n");
            closesocket(sock);
            goto _badExit;
        }
        Buffer[retval] = 0;
        tempdata.i = getMsgCode(Buffer, retval);
        if(msgCode == 510 || msgCode == 511)
        {
            if(tempdata.i == 200)
            {
                if(msgCode == 510)
                    uploadFile(fileTemp);
                else
                    downloadFile(fileTemp, ntohl(*((int*)(Buffer + 2))));
                retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0);
                tempdata.i = getMsgCode(Buffer, retval);
            }
            fclose(fileTemp);
            fileTemp = NULL;
        }
        else if(msgCode == 105)
        {
            break;
        }
        else if(msgCode == 524 && tempdata.i == 200)
        {
            printf("got this hash: ");
            for(tempdata.i = 2; tempdata.i < 18; tempdata.i++)
                printf("%02X", ((uint8_t*)Buffer)[tempdata.i]);
            printf("\n");
            continue;
        }
        else if(msgCode == 523 && tempdata.i == 200)
        {
            // TODO: fix uint64_t endian
            tempdata.l = *((uint64_t*)(Buffer + 2));
#ifdef WIN32
            printf("File size is %I64u\n", tempdata.l);
#else
            printf("File size is %llu\n", tempdata.l);
#endif
            continue;
        }
        else if(tempdata.i == 900)
        {
            if ((retval = sendMessage(200, NULL, 0)) == SOCKET_ERROR)
            {
                fprintf(stderr,"Client: send() failed.\n");
                goto _badExit;
            }
            if ((retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == SOCKET_ERROR)
            {
                fprintf(stderr,"Client: recv() failed.\n");
                closesocket(sock);
                goto _badExit;
            }
            continue;
        }
        while(tempdata.i == 201)
        {
            printf("%s", Buffer + sizeof(msgCode));
            if ((retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == SOCKET_ERROR)
            {
                fprintf(stderr,"Client: recv() failed.\n");
                closesocket(sock);
                goto _badExit;
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
    struct {
        uint32_t blockNum;
        uint16_t size;
        uint8_t md5Res[16];
        uint8_t dataFile[0x200];
    } data;
    char Buffer[BUFFER_SERVER_SIZE];
    int blocksCount, i;
    uint8_t* blocks; // 1 - bad, 0 - good
    int flag = 1;
    for(data.blockNum = 0; (data.size = fread(data.dataFile, 1, 0x200, file)); data.blockNum++)
    {
        MD5(data.dataFile, data.size, data.md5Res);
        data.blockNum = htonl(data.blockNum);
        data.size = htons(data.size);
        sendMessage(210, (char*)&data, 32 + data.size);
        data.blockNum = ntohl(data.blockNum);
    }
    blocksCount = data.blockNum;
    blocks = (uint8_t*)malloc(blocksCount);
    memset(blocks, 1, blocksCount);
    while(flag)
    {
        if(getMsgCode(Buffer, recv(sock, Buffer, BUFFER_SERVER_SIZE, 0)) == 200)
        {
            blocks[(int)*(Buffer + 2)] = 0;
            flag = 0;
            for(i = 0; !flag && i < blocksCount; i++)
                flag = flag || blocks[i]; // only if the whole array it 0 (we finished) than flag will be 0
        }
        else
        {
            data.blockNum = *(Buffer + 2);
            fseek(file, data.blockNum * 0x200, SEEK_SET);
            data.size = fread(data.dataFile, 1, 0x200, file);
            MD5(data.dataFile, data.size, data.md5Res);

            data.blockNum = htonl(data.blockNum);
            data.size = htons(data.size);
            sendMessage(210, (char*)&data, 32 + data.size);
            data.blockNum = ntohl(data.blockNum);
        }
    }
    free(blocks);
    sendMessage(213, NULL, 0);
}

void downloadFile(FILE* file, int blockCount)
{
    struct {
        uint32_t blockNum;
        uint16_t size;
        uint8_t md5Res[16];
        uint8_t dataFile[0x200];
    } data;
    char Buffer[BUFFER_SERVER_SIZE];
    int range[2] = {0, blockCount};
    sendMessage(211, (char*)range, 8);
    int i;
    uint8_t* blocks = (uint8_t*)malloc(blockCount); // 1 - bad, 0 - good
    uint8_t md5Res[MD5_DIGEST_LENGTH];
    int flag = 1;
    while(flag)
    {
        i = recv(sock, (char*)&Buffer, sizeof(Buffer), 0);
        memcpy(&data, Buffer + 2, i - 2);
        data.blockNum = ntohl(data.blockNum);
        data.size = ntohs(data.size);
        MD5(data.dataFile, data.size, md5Res);
        if(memcmp(data.md5Res, md5Res, MD5_DIGEST_LENGTH)) // not equal
        {
            data.blockNum = htonl(data.blockNum);
            sendMessage(212, (char*)&data.blockNum, sizeof(data.blockNum)); // ask for block
        }
        else
        {
            fseek(file, data.blockNum * 0x200, SEEK_SET);
            i = fwrite(data.dataFile, 1, data.size, file);
            blocks[data.blockNum] = 0;
            flag = 0;
            for(i = 0; !flag && i < blockCount; i++)
                flag = flag || blocks[i]; // only if the whole array it 0 (we finished) than flag will be 0
        }
    }

    sendMessage(213, NULL, 0);
}
