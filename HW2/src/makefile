CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`
PTHREAD = -pthread
CLIENT = client.cpp
SERVER = server.cpp
PLAY = test.cpp
CLI = client
SER = server
TEST = test
all: server client
  
server: $(SERVER)
	$(CC) $(SERVER) -std=c++11 -o $(SER) $(OPENCV) $(PTHREAD) 
client: $(CLIENT)
	$(CC) $(CLIENT) -std=c++11 -o $(CLI) $(OPENCV) $(PTHREAD)
test_play: $(PLAY)
	$(CC) $(PLAY) -o $(TEST) $(OPENCV)
.PHONY: clean

clean:
	rm $(CLI) $(SER)
