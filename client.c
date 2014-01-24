#ifdef WIN32
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
#include "defenitions.h"
#include "md5.h"

#define DEFAULT_PORT 7777
#define BUFFER_SERVER_SIZE 0x400 // = 1024

SOCKET sock;

int sendMessage(short msgCode, char* data, int datalen);
short getMsgCode(char* data, unsigned int datalen);

int main(int argc, char **argv)
{
    srand(time(NULL));
    char Buffer[BUFFER_SERVER_SIZE + 150];
    // default to localhost
    char *server_name= "localhost";
    unsigned short port = DEFAULT_PORT;
    int retval;
    unsigned int addr;
    struct sockaddr_in server;
    struct hostent *hp;
    short msgCode;
    int len;

    if (argc > 1)
    	server_name = argv[1];

    union {
    	char str[0xFF];
    } tempdata;

#ifdef WIN32
    WSADATA wsaData;
    if ((retval = WSAStartup(0x202, &wsaData)) != 0)
    {
    	fprintf(stderr,"Server: WSAStartup() failed with error %d\n", retval);
    	goto _badExit;
    }
#endif

    // Attempt to detect if we should call gethostbyname() or gethostbyaddr()
    if (isalpha(server_name[0]))
    {   // server address is a name
        hp = gethostbyname(server_name);
    }
    else
    { // Convert nnn.nnn address to a usable one
        addr = inet_addr(server_name);
        hp = gethostbyaddr((char *)&addr, 4, AF_INET);
    }
    if (hp == NULL )
    {
#ifdef WIN32
        fprintf(stderr,"Client: Cannot resolve address \"%s\": Error %d\n", server_name, WSAGetLastError());
#else
        fprintf(stderr,"Client: Cannot resolve address \"%s\"\n", server_name);
#endif
        goto _badExit;
    }

    // Copy the resolved information into the sockaddr_in structure
    memset(&server, 0, sizeof(server));
    memcpy(&(server.sin_addr), hp->h_addr, hp->h_length);
    server.sin_family = hp->h_addrtype;
    server.sin_port = htons(port);
    sock = socket(AF_INET, SOCK_DGRAM, 0); /* Open a socket */

    if (sock <0 )
    {
#ifdef WIN32
        fprintf(stderr,"Client: Error Opening socket: Error %d\n", WSAGetLastError());
#else
        fprintf(stderr,"Client: Error Opening socket\n");
#endif
        goto _badExit;
    }

    printf("Client: Client connecting to: %s.\n", hp->h_name);
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

    while(1)
    {
    	// Buffer = data
    	printf("enter msg code in dec mode:");
    	scanf("%hd", &msgCode);
    	switch (msgCode)
    	{
    	case 105:
    	case 0:
    		len = 0;
    		break;
    	case 100:
    		printf("username: ");
    		scanf("%s", Buffer + 16);
    		printf("password: ");
    		scanf("%s", tempdata.str);
    		md5((byte_t*)tempdata.str, strlen(tempdata.str), Buffer);
    		len = strlen(Buffer + 16) + 16;
    		break;
    	case 520:
    	case 521:
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
        retval = sendMessage(msgCode, Buffer, len);
        if (retval == SOCKET_ERROR)
        {
            fprintf(stderr,"Client: send() failed.\n");
            goto _badExit;
        }
        if(msgCode == 0)
        	return (0);
        retval = recv(sock, Buffer, BUFFER_SERVER_SIZE, 0);
        if (retval == SOCKET_ERROR)
        {
            fprintf(stderr,"Client: recv() failed.\n");
#ifdef WIN32
            closesocket(sock);
#else
            close(sock);
#endif
            goto _badExit;
        }
        msgCode = getMsgCode(Buffer, retval);
        Buffer[retval] = 0;
        printf("got this code: %8d, data: %s\n", msgCode, Buffer + sizeof(msgCode));

    }
#ifdef WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    printf("excellent exit\n");
    return (0);

_badExit:
#ifdef WIN32
    WSACleanup();
#endif
    return (-1);
}

int sendMessage(short msgCode, char* data, int datalen)
{
	static bool_t lockSend = FALSE;
	char* buffer = (char*)malloc(datalen + sizeof(msgCode));
	memcpy(buffer, &msgCode, sizeof(msgCode));
	if(data && datalen > 0)
		memcpy(buffer + sizeof(msgCode), data, datalen);
	while (lockSend) ;
	lockSend = TRUE;
	int retVal = send(sock, buffer, datalen + sizeof(msgCode), 0);
	lockSend = FALSE;
	return (retVal);
}

short getMsgCode(char* data, unsigned int datalen)
{
	short ret;
	if(datalen < sizeof(ret))
		return (-1);
	memcpy(&ret, data, sizeof(ret));
	return (ret);
}
