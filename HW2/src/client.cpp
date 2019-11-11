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
//#include "opencv2/opencv.hpp"

#define MAXDATASIZE 64 // bytes
// send protocal: 63~3:DATA 1:DATA_LENGTH 0:INSTRUCTION
	// INSTRUCTION:: <0:probe, -1:end connection, 1:ls, 2:put, 3:pull, 4:play>
// recv protocal: 63~3:DATA 1:DATA_LENGTH 0:INSTRUCTION
	// INSTRUCTION:: <0:probe, -1:error message, 1:sending data, 2:end of sending, 3:playing frame, 4:End of frame, 5:End of playing>

//#define DEBUG1


using namespace std;
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

int recv_all(const int *fd, const FILE *out_file, const char instruction){
	char buffer[MAXDATASIZE];
	int len, stat;
	buffer[0] = 1;
	while(buffer[0] == 1){
		len = 0;
		memset(buffer, 0, sizeof buffer);
		stat = recv_message(fd, buffer, &len)
		if(stat == -1){
			perror("recv error");
			fprintf(stderr, "error while %d, received data length:%d\n", (int)instruction, len);
			return -1
		}
		if(buffer[0] == -1) { 
			frpintf(stderr, "The ‘%s’ doesn’t exist.\n", buffer+2);
			return 0;
		}
		write(out_file, buffer+2, buffer[1]);
	}
	return 0;
}

void ls(int *fd, string *file_name){
	char message[MAXDATASIZE] = {0};
	int len = MAXDATASIZE;
	message[0] = 1;					// ls command
	message[1] = file_name.length();
	strcpy(message+2, file_name.c_str(), file_name.length());
	if(send_maeesage(fd, message, &len) == -1){
		perror("send message");
		fprintf(stderr, "Command sent error : ls\nlenght sent : %d", len);
		return;
	}
	recv_all(fd, stdout, 1);
	fflush(stdout);
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

	//////////////////////////////


	/// Instruction ///
	bool run = true;
    while(run) {
    	string buffer;
    	string file;
    	string instruction;

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
        	//cout << instruction << " 51 " << endl;
		}else if(instruction.compare("put") == 0){				// put
			if(file.length() == 0){
				fprintf(stderr, "Command format error\n");
				//cout << "56" << endl;
        		continue;
			}
			//cout << instruction << " " << file << " 58 " << endl;
			
		}else if(instruction.compare("play") == 0){				// play
			if(file.length() == 0){
				fprintf(stderr, "Command format error\n");
				//cout << "62" << endl;
        		continue;
			}
			//cout << instruction << " " << file << " 65 " << endl;
		}else if(instruction.compare("get") == 0){				// get
			if(file.length() == 0){
				fprintf(stderr, "Command format error\n");
				//cout << "69" << endl;
        		continue;
			}
			//cout << instruction << " " << file << " 71 " << endl;
		}
		else if(instruction.compare("close") == 0){
			run = false;
		}else{
			fprintf(stderr, "Command format error\n");
			//cout << "75" << endl;
    		continue;
		}
	}
	
	return 0;
}







