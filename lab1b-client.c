//NAME: Khoa Quach
//EMAIL: khoaquachschool@gmail.com
//ID: 105123806
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
// new header files for Project 1a:
#include <termios.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
// new header files for Project 1b:
#include <sys/socket.h>
#include <zlib.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
void print_error(const char* msg);
void reset();
void set();
void write_with_check(int fd, const void *buf, size_t size);
//void read_write(); 
ssize_t read_with_check(int fd, void *buf, size_t size);
void check_close(int fd);
// 1B:
void client_case();
void log_write();
void log_receive();
void exit_compression();
void compression_setup();
void socket_setup();
#define CTRLD 0x04
#define CTRLC 0x03
#define CR 0x0D
#define LF 0x0A
// map received <cr> or <lf> into <cr><lf> 
const char crlf[2] = {'\r','\n'};
char arrow_C[2] = "^C"; 
// Upon receiving an EOF (^D, or 0x04) from the terminal, close the pipe to the shell,
//  but continue processing input from the shell.
char arrow_D[2] = "^D";
struct termios t0;
// 1B:
z_stream c_to_s;
z_stream s_to_c;
struct sockaddr_in serveraddr;
struct hostent* server;
int sockfd;
int log_fd;
int port_number;
int compress_flag = 0;
int log_flag = 0;
int port_flag = 0; // mandatory


// 1B:
/*--------------------compression_setup()--------------------------*/
void exit_compression() {
	deflateEnd(&c_to_s);
	inflateEnd(&s_to_c);
}
// https://zlib.net/zpipe.c
void compression_setup() {
	atexit(exit_compression);
	c_to_s.zalloc = Z_NULL;
	c_to_s.zfree = Z_NULL;
	c_to_s.opaque = Z_NULL;
	if (deflateInit(&c_to_s, Z_DEFAULT_COMPRESSION) != Z_OK) {
		print_error("Error: deflateInit error (allocate deflate state problem)");
	}
	s_to_c.zalloc = Z_NULL;
	s_to_c.zfree = Z_NULL;
	s_to_c.opaque = Z_NULL;
	if (inflateInit(&s_to_c) != Z_OK) {
		print_error("Error: inflateInit error (allocate inflate state problem)");
	}
}
/*------------------------socket_setup()-----------------------------------*/
// https://bit.ly/3oIKwRe
void socket_setup() {
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		print_error("Error: opening the socket");
	server = gethostbyname("localhost"); // 127.0.0.1
	if (server == NULL) {
		print_error("Error: problem trying to get/find host information or no such host.");
	}
	/* build the server*/
	bzero((char*)&serveraddr, sizeof(serveraddr));
	// important
	serveraddr.sin_family = AF_INET;
	bcopy((char*)server->h_addr,
		(char*)&serveraddr.sin_addr.s_addr, server->h_length);
	// important
	serveraddr.sin_port = htons(port_number);
	/* connection with server */
	if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
		print_error("Error: connection error !");
}
/*------------------------error message helper function --------*/
void print_error(const char* msg){
	fprintf(stderr, "%s , error number: %d, strerror: %s\n ",msg, errno, strerror(errno));
    exit(1);
}
/*------------------input mode non canonical setting ----------*/
/*
put the keyboard (the file open on file descriptor 0) into character-at-a-time, 
no-echo mode (also known as non-canonical input mode with no echo). 
it is suggested that you get the current terminal modes, save them for restoration, 
and then make a copy with only the following changes:*/
// https://www.gnu.org/software/libc/manual/html_node/Noncanon-Example.html
// but I used TCSANOW instead of TCSAFLUSH
void reset(){
	if (tcsetattr(0, TCSANOW, &t0) < 0)
		print_error("Error: could not restore the terminal attributes");
}

void set(){ // 1B can reuse this code
	struct termios t1;
	// Double check
	int status = isatty(STDIN_FILENO);
	if (!status){
		print_error("This is not a terminal!");
	}
	// save
	tcgetattr(0,&t0);
	atexit(reset); // THIS IS CRUCIAL, saves you from repeating code
	// Set new
	tcgetattr(0,&t1);
	t1.c_iflag = ISTRIP;	/* only lower 7 bits*/
	t1.c_oflag = 0;		/* no processing	*/
	t1.c_lflag = 0;		/* no processing	*/
	t1.c_cc[VMIN] = 1;
	t1.c_cc[VTIME] = 0;
	// new
	if(tcsetattr(0,TCSANOW, &t1) < 0){
		print_error("Error: setting new attributes!");
	}
}
/*-----------------------Regular writing (no --shell) ----------------*/
// spec suggests using 256 bytes,inspired from my function in lab0
// saves time rewriting for the checking if there's a write error
void write_with_check(int fd, const void *buf, size_t size){  // 1B can reuse this code
	if(write(fd,buf,size) < 0){ 
		print_error("Error: writing error");
	} // Note to self: when passing in as function, it is just the name not &buf
} // https://www.geeksforgeeks.org/input-output-system-calls-c-create-open-close-read-write/

ssize_t read_with_check(int fd, void *buf, size_t size) // 1B can reuse this code
{
	ssize_t s = read(fd,buf,size);
	if(s < 0){
		print_error(("Error: reading error"));
	}
	return s;
}

// no need for pipes from client to server, firrst two are connected via TCP connection
// no need for child_case() code
void check_close(int fd){ // 1B can reuse this code
	if (close(fd) < 0){
		print_error("Error: close error during I/O redireciton");
	}
}

void close_exit() {
	check_close(sockfd);
	check_close(log_fd);
}

// 1B:
void log_write(const void *buf, size_t size)
{
	char buffer[256];
	int byte_size = size;
	sprintf(buffer, "%d",byte_size);
	int byte_length = strlen(buffer);
	write_with_check(log_fd,"SENT ",strlen("SENT "));
	write_with_check(log_fd,buffer,byte_length);
	write_with_check(log_fd," bytes: ",strlen(" bytes: "));
	write_with_check(log_fd,buf,size);
	write_with_check(log_fd,"\n",sizeof(char)*1);
}
// http://www.cplusplus.com/reference/cstdio/sprintf/
void log_receive(const void *buf, size_t size )
{
	char buffer[256];
	int byte_size = size;
	sprintf(buffer, "%d",byte_size);
	int byte_length = strlen(buffer);
	write_with_check(log_fd,"RECEIVED ",strlen("RECEIVED "));
	write_with_check(log_fd,buffer,byte_length);
	write_with_check(log_fd," bytes: ",strlen(" bytes: "));
	write_with_check(log_fd,buf,size);
	write_with_check(log_fd,"\n",sizeof(char)*1);
}

void client_case() {
	short pevents = POLLIN | POLLERR | POLLHUP;
	struct pollfd fds[2] = {
			{0, pevents, 0},
			{sockfd, pevents, 0}
	};
	char c;
	char buf[256];
	ssize_t s;
	while (1) {
		int ret = poll(fds, 2, 0); // poll(fds,2,-1);
		if (ret < 0) {
			print_error("Error: polling");
		}
		if (ret > 0) {
			// returned events
			short stdin_e = fds[0].revents;
			short socket_e = fds[1].revents;
			// INPUT KEYBOARD
			/*
			If a ^D is entered on the terminal, simply pass it through to the server like any other character.
				if a ^C is entered on the terminal, simply pass it through to the server like any other character.
			*/
			if (stdin_e & POLLIN) {
				s = read_with_check(0,buf,sizeof(char)*256);
				if(!compress_flag)
				{
					for (int i =0; i < s; i++){
						c = buf[i];
						if (c == CR || c == LF || c == crlf[0] || c == crlf[1]) {
							write_with_check(1, &crlf, sizeof(char) * 2);
							//write_with_check(sockfd,&c,1);
							write_with_check(sockfd,&crlf[1],1);
						//} else if (c == CTRLD || c == 0x04) {
						//	write_with_check(1, &arrow_D, sizeof(char) * 2);
						//}
						//else if (c == CTRLC || c == 0x03) {
						//	write_with_check(1, &arrow_C, sizeof(char) * 2);
						}else {
							write_with_check(1, &c, sizeof(char) * 1);
							write_with_check(sockfd,&c,sizeof(char) * 1);
						}
					}
					if(log_flag){
						log_write(buf,s);
					}
				}
				if(compress_flag){
					int compressed_bytes;
					unsigned char compressed_buf[1024];
					c_to_s.avail_in = s;
					c_to_s.next_in = (unsigned char *)buf;
					c_to_s.avail_out = 1024;
					c_to_s.next_out = compressed_buf;
					do {
						if (deflate(&c_to_s, Z_SYNC_FLUSH) != Z_OK) {
							print_error("Error: deflate error");
						}
					} while (c_to_s.avail_in > 0);
					compressed_bytes = 1024 - c_to_s.avail_out;
					write_with_check(sockfd,compressed_buf,compressed_bytes);
					for (int i =0; i < s; i++){
						c = buf[i];
						if (c == CR || c == LF || c == crlf[0] || c == crlf[1]) {
							write_with_check(1, &crlf, sizeof(char) * 2);
						//} else if (c == CTRLD || c == 0x04) {
						//	write_with_check(1, &arrow_D, sizeof(char) * 2);
						//}
						//else if (c == CTRLC || c == 0x03) {
						//	write_with_check(1, &arrow_C, sizeof(char) * 2);
						}else {
							write_with_check(1, &c, sizeof(char) * 1);
						}
					}
					if(log_flag){
						log_write(compressed_buf,compressed_bytes);
					}
				}
				
			} // stdin_e && POLLIN
			/*--------------------------socket_e----------------------------------------*/
			if (socket_e & POLLIN) {
				s = read_with_check(sockfd,buf,sizeof(char)*256);
				if(s == 0)
					exit(0);
				if (log_flag){
					log_receive(buf,s);
				}
				if (!compress_flag){
					for (int i =0; i < s; i++){
						c = buf[i];
						if (c == CR || c == LF || c == crlf[0] || c == crlf[1]) {
							write_with_check(1, &crlf, sizeof(char) * 2);
						} else {
							write_with_check(1, &c, sizeof(char) * 1);
						}
					}
				}
				if(compress_flag){
					int decompressed_bytes;
					unsigned char decompressed_buf[1024];
					s_to_c.avail_in = s; // BUF 
					s_to_c.next_in = (unsigned char *)buf; // BUF
					s_to_c.avail_out = 1024; // SOCKETBUF SIZE
					s_to_c.next_out = decompressed_buf; // SOCKETBUF
					// inflate
					do {
						if (inflate(&s_to_c, Z_SYNC_FLUSH) != Z_OK){
							print_error("Error: inflate error");
						}
					} while (s_to_c.avail_in > 0);
					decompressed_bytes = 1024 - s_to_c.avail_out;
					for (int i =0; i < decompressed_bytes; i++){
						c = decompressed_buf[i];
						if (c == CR || c == LF || c == crlf[0] || c == crlf[1]) {
							write_with_check(1, &crlf, sizeof(char) * 2);
						} else {
							write_with_check(1, &c, sizeof(char) * 1);
						}
					}
				}
			}
			if ((POLLHUP | POLLERR) & socket_e) {
				exit(0);
			} // end POLLUP POLLERR
			if ((POLLHUP | POLLERR) & stdin_e) {
				exit(1);
			} // end POLLUP POLLERR
		}
	}
}


int main(int argc, char **argv){
	int choice;
	// 1B:
	// I learned that you don't need to declare flags outside of the getopt struct
	// but I kept it like this to be consistent with lab1a
	/*
	The client program will open a connection to a server (port specified with the mandatory --port= command line parameter) 
	rather than sending it directly to a shell. 
	*/
	static struct option long_options[] = 
	{
		{"port", 1, 0, 'p'}, 
		{"log",1,0,'l'},
		{"compress",0,0,'c'},
		{0,0,0,0}
	};
	while((choice = getopt_long(argc, argv, "", long_options,NULL))!= -1)
	{
		switch(choice){
			case 'p':
				port_number = atoi(optarg);
				port_flag = 1;
				break;
			case 'l':
				log_flag = 1;
				log_fd= creat(optarg, 0666);
				if (log_fd == -1)
					print_error("Error: create log file");
				break;
			case 'c':
				compress_flag = 1;
				break;
			case '?':
				fprintf(stderr, "unrecognized argument! Correct usage: ./lab1b --port=# --log=file.txt --compress, where # is an integer greater than 1024\n");
				exit(1);
			default:
				fprintf(stderr, "unrecognized argument! Correct usage: ./lab1b --port=# --log=file.txt --compress, where # is an integer greater than 1024\n");
				exit(1);
		}// switch
	}// while
	if (port_flag != 1)
		print_error("Error: --port= is required! Specify a port number greater than 1024");
	if (port_number < 1024)
		print_error("Error: --port=port_number , Please choose a port number greater than 1024");
	if (compress_flag)
		compression_setup();
	set(); // set up non canonical input mode
	socket_setup();
	client_case();
	atexit(close_exit);
	exit(0);
}
