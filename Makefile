CFLAGS = -Wall -Wextra -g -lm

build: server subscriber

subscriber: subscriber.cpp
	g++ $(CFLAGS) -std=c++17 subscriber.cpp -o subscriber

server: server.cpp
	g++ $(CFLAGS) -std=c++17 server.cpp -o server

clean:
	rm -f server subscriber