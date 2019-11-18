#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <dirent.h>
//#include "opencv2/opencv.hpp"
#define MAXDATASIZE 1024 	// bytes
// send protocal: 63~3:DATA 2~1:DATALENGTH 0:INSTRUCTION
	// INSTRUCTION:: <0:probe, -1:end connection, 1:preparing, 2:sending, 3:end send, 4:ask for frame>
	// command >> 1:ls, 2:put, 3:get, 4:play, 5:close connection
// recv protocal: 63~3:DATA 2~1:DATALENGTH 0:INSTRUCTION
	// INSTRUCTION:: <0:probe, -1:error, 1:preparing, 2:sending, 3:end send, 4:send a frame>

#define BACKLOG 20 		// 有多少個特定的連線佇列（pending connections queue）
#define Server "./server_files"
//#define DEBUG2
using namespace std;

//using namespace cv;

int recv_message(int *fd, char *message, int len){
	int total = 0;
	int bytesleft = len;
	int n;
	while(bytesleft > 0){
		n = recv(*fd, message+total, bytesleft, 0);
		if(n == -1 || n == 0) { return -1; }
		total += n;
		bytesleft -= n;
	}
	return 0;
}

int send_message(int *fd, char *message, int len){
	int total = 0; 			// 我們已經送出多少 bytes 的資料
	int bytesleft = len; 	// 我們還有多少資料要送
	int n;
	while(total < len) {
		n = send(*fd, message+total, bytesleft, 0);
		if (n == -1 || n == 0) { return -1; }
		total += n;
		bytesleft -= n;
	}
	return 0;
}

int recv_file(int *fd, FILE *out_file, char message_buffer[MAXDATASIZE]){	// to file, ex:ls write to terminal, puts and gets
	int stat = recv_message(fd, message_buffer, MAXDATASIZE);
	unsigned short message_len, temp;
	while(stat == 0){
		if(message_buffer[0] == 2){				// receiving
			message_len = message_buffer[1];
			temp = message_buffer[2];
			message_len = (message_len | (temp << 8));
			cout << "received" << message_len << "words\n";
			fwrite(message_buffer+3, 1, message_len, out_file);
		}else if(message_buffer[0] == 3){		// end
			return 0;
		}else{									// error?
			return -2;
		}
		stat = recv_message(fd, message_buffer, MAXDATASIZE);
		if(stat == -1){
			perror("recv error");
			return -1;
		}
	}
	return 0;
}
int recv_words(int *fd, string *words, char message_buffer[MAXDATASIZE]){	// to memory, ex:file names, frame size, etc.
	words->clear();
	int stat = recv_message(fd, message_buffer, MAXDATASIZE);
	unsigned short message_len, temp;
	while(stat == 0){
		if(message_buffer[0] == 2){				// receiving
			message_len = message_buffer[1];
			temp = message_buffer[2];
			message_len = (message_len | (temp << 8));
			words->append(message_buffer+3, message_len);
		}else if(message_buffer[0] == 3){		// end
			return 0;
		}else{									// error?
			return -2;
		}
		stat = recv_message(fd, message_buffer, MAXDATASIZE);
		if(stat == -1){
			perror("recv error");
			return -1;
		}
	}
	return 0;
}

int send_words(int *fd, stringstream *words, char message_buffer[MAXDATASIZE]){
	memset(message_buffer, 0, MAXDATASIZE);
	message_buffer[0] = 2;
	int len = 0;
	words->read(message_buffer+3, MAXDATASIZE-3);
	while((len = words->gcount()) != 0){
		message_buffer[1] = (len & 511);
		message_buffer[2] = (len >> 8);
		if(send_message(fd, message_buffer, MAXDATASIZE) == -1){
			return -1;
		}
		words->read(message_buffer+3, MAXDATASIZE-3);
	}
	// end sending
	message_buffer[0] = 3;
	send_message(fd, message_buffer, MAXDATASIZE);
	return 0;
}

int send_file(int *fd, FILE *file, char message_buffer[MAXDATASIZE]){
	memset(message_buffer, 0, MAXDATASIZE);
	message_buffer[0] = 2;
	unsigned short len = fread(message_buffer+3, 1, MAXDATASIZE-3, file);
	while(len != 0){
		message_buffer[1] = (len & 511);
		message_buffer[2] = (len >> 8);
		if(send_message(fd, message_buffer, MAXDATASIZE) == -1){
			return -1;
		}
		len = fread(message_buffer+3, 1, MAXDATASIZE-3, file);
	}
	message_buffer[0] = 3;
	send_message(fd, message_buffer, MAXDATASIZE);
	return 0;
}

void ls(int *fd, char message_buffer[MAXDATASIZE]){
	// prepare
	
	DIR *dp = NULL;
	struct dirent *dptr = NULL;
	dp = opendir(".");
	stringstream container;
	cout << "ls command\n";
	while((dptr = readdir(dp)) != NULL){
		string filenameStr(dptr->d_name);
		cout << filenameStr << endl;
		container << filenameStr;
		container << "\n";
	}

	// send
	send_words(fd, &container, message_buffer);
	return;
}

void answer_YesNo(int *fd, char message_buffer[MAXDATASIZE], bool y_n){
	message_buffer[0] = 1;
	message_buffer[3] = (y_n)?1:-1;
	send_message(fd, message_buffer, MAXDATASIZE);
}

void put(int *fd, char message_buffer[MAXDATASIZE]){
	// get file name
	string filenameStr;
	if(recv_words(fd, &filenameStr, message_buffer) != 0){
		perror("put filename");
		return;
	}

	remove(filenameStr.c_str());			// prevent overlapping
	FILE *out_file = fopen(filenameStr.c_str(), "wb");

	recv_file(fd, out_file, message_buffer);
	fclose(out_file);
	return;
}

void get(int  *fd){

}



void command_handle(int *fd){
	char message_buffer[MAXDATASIZE];

	while(1){
		cout << "receiving command\n";
		if(recv_message(fd, message_buffer, MAXDATASIZE) == -1) {return;}	// error
		if(message_buffer[0] != 1){
			break;
		}
		if(message_buffer[3] == 1){		// ls
			answer_YesNo(fd, message_buffer, true);
			ls(fd, message_buffer);
		}else if(message_buffer[3] == 2){	// put
			answer_YesNo(fd, message_buffer, true);
			put(fd, message_buffer);
		}else if(message_buffer[3] == 3){	// get
			answer_YesNo(fd, message_buffer, true);
		}else if(message_buffer[3] == 4){	// play
			answer_YesNo(fd, message_buffer, true);
		}else if(message_buffer[3] == 5){	// close
			close(*fd);
			return;
		}else{								// unknown command
			message_buffer[0] = -1;
			send_message(fd, message_buffer, MAXDATASIZE);
		}
		#ifndef DEBUG2
		int count;
		ioctl(sockfd, FIONREAD, &count);
		cout << "data in socket remains : " << count << "bytes\n";
    	#endif
	}
	// error occur
	message_buffer[0] = -1;
	send_message(fd, message_buffer, MAXDATASIZE);
	close(*fd);
}

int main(int argc , char **argv){
	
	if(argc != 2){
		fprintf(stderr, "usage : ./server ip:port\n");
		return 0;
	}

	/// folder create
	struct stat s;
	int err = stat(Server, &s);
	if(-1 == err) {
		if(ENOENT == errno) {
			mkdir(Server, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		} else {
			perror("stat");
			exit(1);
		}
	} else {
		if(S_ISDIR(s.st_mode)) {
			cerr << "server file already exist\n";
		} else {
			cerr << "fail to create server file, eist same file with same name\n";
			exit(1);
		}
	}
	chdir(Server);
	/// TCP connection section ///
	int sockfd, new_fd, yes = 1;
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
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
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

	freeaddrinfo(servinfo);

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		return 2;
	}

	//////////////////////////////

	while(1) { // 主要的 accept() 迴圈
		printf("server: waiting for connections...\n");
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		// pthread handler and command handler
		cout << "new client incoming\n";
		command_handle(&new_fd);
		cout << "end connection\n";
	}
	return 0;	
}







