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
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
//#include "opencv2/opencv.hpp"

#define MAXDATASIZE 1024 // bytes
// send protocal: 63~3:DATA 2~1:DATALENGTH 0:INSTRUCTION
	// INSTRUCTION:: <0:probe, -1:end connection, 1:preparing, 2:sending, 3:end send, 4:ask for frame>
	// command >> 1:ls, 2:put, 3:get, 4:play 5:close
// recv protocal: 63~3:DATA 2~1:DATALENGTH 0:INSTRUCTION
	// INSTRUCTION:: <0:probe, -1:error, 1:preparing, 2:sending, 3:end send, 4:send a frame>

#define Client "./client_files"
#define DEBUG1
//#define DEBUG2
using namespace std;
//using namespace cv;

char message_buffer[MAXDATASIZE];
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

int send_message( int*  fd, char *message, int len){
	int total = 0; 				// 我們已經送出多少 bytes 的資料
	int bytesleft = len; 		// 我們還有多少資料要送
	int n;
	while(total < len) {
		n = send(*fd, message+total, bytesleft, 0);
		if (n == -1 || n == 0) { return -1; }
		total += n;
		bytesleft -= n;
	}
	return 0;
}

int recv_file(int *fd, FILE *out_file){							// to file, ex:ls write to terminal, put and get
	int stat = recv_message(fd, message_buffer, MAXDATASIZE);
	unsigned short message_len, temp;
	while(stat == 0){
		if(message_buffer[0] == 2){				// receiving
			message_len = message_buffer[1];
			temp = message_buffer[2];
			message_len = (message_len | (temp << 8));
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
int recv_words(int *fd, string *words){							// to memory, ex:file names, frame size, etc.
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

int send_words(int *fd, stringstream *words){
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
	message_buffer[0] = 3;
	send_message(fd, message_buffer, MAXDATASIZE);
	return 0;
}

int send_file(int *fd, FILE *file){
	memset(message_buffer, 0, MAXDATASIZE);
	message_buffer[0] = 2;
	unsigned short len = fread(message_buffer+3, 1, MAXDATASIZE-3, file);
	while(len != 0){
		message_buffer[1] = (len & 511);
		message_buffer[2] = (len >> 8);
		if(send_message(fd, message_buffer, MAXDATASIZE) == -1){
			return -1;
		}
		cout << "sent " << len << " words\n";
		len = fread(message_buffer+3, 1, MAXDATASIZE-3, file);
	}
	message_buffer[0] = 3;
	send_message(fd, message_buffer, MAXDATASIZE);
	return 0;
}

void ls(int *fd){
	memset(message_buffer, 0, sizeof message_buffer);

	message_buffer[0] = 1;					// send ls command
	message_buffer[3] = 1;
	if(send_message(fd, message_buffer, MAXDATASIZE) == -1){
		perror("send_message");
		return;
	}

	// receive answer from server
	if(recv_message(fd, message_buffer, MAXDATASIZE) != 0){
		if(!(message_buffer[0] == 1 && message_buffer[3] == 1)){
			perror("ls rejected");
			return;
		}
	}
	cout << "===================" << endl;
	if(message_buffer[0] == 1 && message_buffer[3] == 1){	// command accepted
		// write to stdout
		recv_file(fd, stdout);
		fflush(stdout);
	}
	cout << "===================" << endl;
	return;
}

void put(int *fd, string * file_name){
	FILE *in_file = fopen(file_name->c_str(), "rb");
	if(in_file == NULL){					// check if file exist
		cout << "The ‘" << *file_name << "’ doesn’t exist.\n";
		return;
	}

	message_buffer[0] = 1;					// send put command
	message_buffer[3] = 2;
	if(send_message(fd, message_buffer, MAXDATASIZE) == -1){
		perror("send_message");
		return;
	}
	// receive answer from server
	if(recv_message(fd, message_buffer, MAXDATASIZE) != 0){
		return;
	}
	if(message_buffer[0] == 1 && message_buffer[3] == 1){	// command accepted
		// write to stdout
		stringstream ss;
		ss << (*file_name);
		if(send_words(fd, &ss) != 0) {return;}
		send_file(fd, in_file);
	}
	
	return;
}

void get(int *fd){
/*
	remove(message_buffer+2);			// prevent overlapping
	FILE *out_file = fopen(message_buffer+2, "wb");

	/// send message to start sending file
	memset(message_buffer, 0, sizeof message_buffer);
	message_buffer[0] = 1;
	send_message(fd, message_buffer, MAXDATASIZE);

	recv_all(fd, out_file, MAXDATASIZE);
	fclose(out_file);*/
}

int main(int argc , char **argv){
	
	if(argc != 2){
		fprintf(stderr, "usage : ./client ip:port\n");
		return 0;
	}

	string ip_and_port(argv[1]);
	int pos = ip_and_port.find_first_of(':');

	if(pos >= ip_and_port.length()){
		fprintf(stderr, "usage : ./client ip:port\n");
		return 0;
	}

	string PORT = ip_and_port.substr(pos + 1);
    string IP = ip_and_port.substr(0, pos);

	if(PORT.length() == 0 || IP.length() == 0){
		fprintf(stderr, "usage : ./client ip:port\n");
		return 0;
	}
	/// folder create
	struct stat s;
	int err = stat(Client, &s);
	if(-1 == err) {
		if(ENOENT == errno) {
			mkdir(Client, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		} else {
			perror("stat");
			exit(1);
		}
	} else {
		if(S_ISDIR(s.st_mode)) {
			cerr << "client file already exist\n";
		} else {
			cerr << "fail to create client file, eist same file with same name\n";
			exit(1);
		}
	}
	chdir(Client);
	/// TCP connection section ///
	int sockfd;
	unsigned char data_buffer[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char ipstr[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;		// IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;	// TCP stream socket
	//hints.ai_flags = AI_PASSIVE; 		// Let Computer to decide my IP 

	rv = getaddrinfo(IP.c_str(), PORT.c_str(), &hints, &servinfo);
	if (rv != 0) {
		//fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		fprintf(stderr, "getaddrinfo error\n");
		return(2);
	}
	
	#ifndef DEBUG1
	fprintf(stderr, "IP addresses for %s :\n\n" , IP.c_str());
	
	for(p = servinfo ; p != NULL ; p = p->ai_next) {
		void *addr;
		char ipver[5];

		// 取得本身位址的指標，
		// 在 IPv4 與 IPv6 中的欄位不同：
		if (p->ai_family == AF_INET) { 		// IPv4
			struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
			addr = &(ipv4->sin_addr);
			strcpy(ipver, "IPv4");
		} else { 							// IPv6
			struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
			addr = &(ipv6->sin6_addr);
			strcpy(ipver, "IPv6");
		}

		// convert the IP to a string and print it:
		inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
		fprintf(stderr,"%s: %s\n", ipver, ipstr);
	}

	#endif
	p = servinfo;
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return (2);
	}

	if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
		perror("client: socket");
		return(2);
	}

	if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
		close(sockfd);
    	perror("client: connect");
    	return(2);
	}

	freeaddrinfo(servinfo);
	cout << "connection success\n";

	/// Instruction ///
	bool run = true;
    while(run) {
    	cout << "waiting for command...\n";
    	string buffer;
    	string file;
    	string instruction;
    	#ifndef DEBUG2
		int count;
		ioctl(sockfd, FIONREAD, &count);
		cout << "data in socket remains : " << count << "bytes\n";
    	#endif

    	getline(cin, buffer);
    	if(buffer.length() == 0)continue;
        //cout << "Buffer : " << buffer << endl;
        istringstream ss(buffer);
		if(ss){ss >> instruction;}
		if(ss){ss >> file;}

    	if(instruction.compare("ls") == 0){						// ls
			if(file.length() != 0){
				fprintf(stderr, "Command format error\n");
        		continue;
        	}
        	ls(&sockfd);
		}else if(instruction.compare("put") == 0){				// put
			if(file.length() == 0){
				fprintf(stderr, "Command format error\n");
        		continue;
			}
			put(&sockfd, &file);
		}else if(instruction.compare("play") == 0){				// play
			if(file.length() == 0){
				fprintf(stderr, "Command format error\n");
        		continue;
			}
			
		}else if(instruction.compare("get") == 0){				// get
			if(file.length() == 0){
				fprintf(stderr, "Command format error\n");
        		continue;
			}

		}else if(instruction.compare("close") == 0){				// close
			run = false;
			message_buffer[0] = 1;
			message_buffer[3] = 5;
			send_message(&sockfd, message_buffer, MAXDATASIZE);
		}else{
			fprintf(stderr, "Command format error\n");
			//cout << "75" << endl;
    		continue;
		}
	}
	
	return 0;
}







