CC=gcc
CFLAGS=-c -Wall
LDFLAGS=
SOURCES=client.c md5.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=aftp_client
LIBS=
REMOVEFILECOMMAND=rm -f

ifeq ($(OS),Windows_NT)
	LIBS = -lws2_32
endif

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clear: clean

clean:
	$(REMOVEFILECOMMAND) *.o $(EXECUTABLE)
