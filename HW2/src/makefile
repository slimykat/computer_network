CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`
PTHREAD = -pthread
STD = `-std=c++17`
CLIENT = client.cpp
SERVER = server.cpp
CLI = client
SER = server

all: server client
  
server: $(SERVER)
	$(CC) $(SERVER) -o $(SER) $(STD) $(OPENCV) $(PTHREAD) 
client: $(CLIENT)
	$(CC) $(CLIENT) -o $(CLI) $(STD) $(OPENCV) $(PTHREAD)

.PHONY: clean

clean:
	rm $(CLI) $(SER)