#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <filesystem>
//#include "opencv2/opencv.hpp"
#define MAXDATASIZE 64 	// bytes
// send protocal: 63~3:DATA 1:DATA_LENGTH 0:INSTRUCTION
	// INSTRUCTION:: <0:probe, -1:end connection, 1:ls, 2:put, 3:pull, 4:play>
// recv protocal: 63~3:DATA 1:DATA_LENGTH 0:INSTRUCTION
	// INSTRUCTION:: <0:probe, -1:error message, 1:sending data, 2:end of sending, 3:playing, 4:End of playing>

#define BACKLOG 20 		// 有多少個特定的連線佇列（pending connections queue）
#define Server "server_file"
using namespace std;
namespace fs = std::filesystem;
//using namespace cv;

int recv_message(int *fd, char *message, int *len){
	int total = 0;
	int bytesleft = MAXDATASIZE;
	int n;
	while(bytesleft > 0){
		n = recv(*fd, message+total, bytesleft, 0);
		if(n == -1) { break; }
		total += n;
		bytesleft -= n;
	}
	*len = total;

	return ((n==-1)?-1:0);
}


int send_maeesage(int *fd, char *message, int *len){
	int total = 0; // 我們已經送出多少 bytes 的資料
	int bytesleft = *len; // 我們還有多少資料要送
	int n;

	while(total < *len) {
		n = send(*fd, message+total, bytesleft, 0);
		if (n == -1) { break; }
		total += n;
		bytesleft -= n;
	}

	*len = total; // 傳回實際上送出的資料量

	return ((n==-1)?-1:0); // 失敗時傳回 -1、成功時傳回 0

}

void ls(int *fd){


}


void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc , char **argv){
	
	if(argc != 2){
		fprintf(stderr, "usage : ./server ip:port\n");
		return 0;
	}

	/// file create
	if(fs::create_directory(Server) == false){
		fprintf(stderr, "fail to create server file\n");
		return 0;
	}

	/// TCP connection section ///
	int sockfd, new_fd, yes = 1;
	char data_buffer[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // 連線者的位址資訊 
	socklen_t sin_size;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;		// IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;	// TCP stream socket
	hints.ai_flags = AI_PASSIVE; 		//  my IP 


	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// 以迴圈找出全部的結果，並綁定（bind）到第一個能用的結果
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("server: bind");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // 全部都用這個 structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}


	printf("server: waiting for connections...\n");

	//////////////////////////////
	char s[INET6_ADDRSTRLEN];
	int len;
	while(1) { // 主要的 accept() 迴圈
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		len = 0;
		recv_message(new_fd, data_buffer, &len);

		if(data_buffer[0] == 1){		// ls
			
		}else if(data_buffer[0] == 2){	// put
			// not yet
		}else if(data_buffer[0] == 3){	// pull
			// not yet
		}else if(data_buffer[0] == 4){	// play
			// not yet
		}else if(data_buffer[0] == -1){	// error message
			// not yet
		}else if(data_buffer[0] == 0){	// probing, maybe don't need this
			// not yet
		}else{							// error
			perrer("INSTRUCTION error");
			return 0
		}

/*
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (send(new_fd, "Hello, world!", 13, 0) == -1) perror("send");

		close(new_fd);
*/		
	}
	return 0;    
}







