CC=gcc
CFLAGS=-c -Wall
LDFLAGS=
SOURCES=client.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=aftp_client
LIBS=
OPENSSL_LIB=-lssl -lcrypto
REMOVEFILECOMMAND=rm -f

ifeq ($(OS),Windows_NT)
	LIBS = -lws2_32
endif

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS) $(OPENSSL_LIB)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clear: clean

clean:
	$(REMOVEFILECOMMAND) *.o $(EXECUTABLE)
