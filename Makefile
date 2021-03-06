CXX=g++
LIBS=-lssl -lcrypto
CXXFLAGS+=-std=c++0x -c -Wall -Wextra -flto
LDFLAGS+=-flto
SOURCES=client.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=aftp-client
REMOVEFILECOMMAND=rm -f

ifeq ($(OS),Windows_NT)
	LIBS+=-lws2_32
	CXXFLAGS+=-DWIN32
else
	LIBS+=-lpthread
endif

all: $(SOURCES) $(EXECUTABLE)

debug: CXXFLAGS += -DDEBUG -g
debug: LDFLAGS += -DDEBUG -g
debug: $(SOURCES) $(EXECUTABLE)
	
	
$(EXECUTABLE): $(OBJECTS) 
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	$(REMOVEFILECOMMAND) *.o $(EXECUTABLE)*

install:
	install -Dm 755 $(EXECUTABLE) "$(DESTDIR)/usr/bin/$(EXECUTABLE)"

uninstall:
	rm "$(DESTDIR)/usr/bin/$(EXECUTABLE)"
