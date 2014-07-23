CC=gcc
LIBS=-lssl -lcrypto
CFLAGS+=-c -Wall
SOURCES=client.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=aftp-client
REMOVEFILECOMMAND=rm -f

ifeq ($(OS),Windows_NT)
	LIBS+=-lws2_32
	CFLAGS+=-DWIN32
else
	LIBS+=-lpthread
endif

all: $(SOURCES) $(EXECUTABLE)

debug: CFLAGS += -DDEBUG -g
debug: LDFLAGS += -DDEBUG -g
debug: $(SOURCES) $(EXECUTABLE)
	
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(REMOVEFILECOMMAND) *.o $(EXECUTABLE)*

install:
	install -Dm 755 $(EXECUTABLE) "$(DESTDIR)/usr/bin/$(EXECUTABLE)"

uninstall:
	rm "$(DESTDIR)/usr/bin/$(EXECUTABLE)"

