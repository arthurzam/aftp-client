#ifndef DEFENITIONS_H_
#define DEFENITIONS_H_

#include <stdlib.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE !FALSE
#endif

#ifndef USERNAME_LENGTH
#define USERNAME_LENGTH 0x40
#endif

typedef unsigned char bool_t;
typedef unsigned char byte_t;

#ifdef WIN32
typedef int from_len_t;
#else
#include <sys/socket.h>
typedef socklen_t from_len_t;
#endif

#ifndef WIN32
#define SOCKET int
#define SOCKET_ERROR	(-1)
#endif

#endif
