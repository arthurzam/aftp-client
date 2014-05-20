#ifndef DEFENITIONS_H_
#define DEFENITIONS_H_

#include <stdint.h>
#include <stdbool.h>

#ifndef USERNAME_LENGTH
#define USERNAME_LENGTH 0x40
#endif

#ifdef WIN32
typedef int socklen_t;
#else
#define SOCKET int
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

#endif
