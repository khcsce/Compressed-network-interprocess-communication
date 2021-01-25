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
void read_write(); 
ssize_t read_with_check(int fd, void *buf, size_t size);
void shell_exit();
void make_pipe(int p[2]);
void signal_handler(int sig);
void check_dup2(int old, int new);
void check_close(int fd);
void child_case();
void parent_case();

#define CTRLD 0x04
#define CTRLC 0x03
#define CR 0x0D
#define LF 0x0A

const char crlf[2] = {'\r','\n'};
char arrow_C[2] = "^C"; 
char arrow_D[2] = "^D";
int p_to_c[2]; // to the child // parent pipe
int c_to_p[2]; // to the parent // child pipe
pid_t pid; // will be set to fork for creating a new process 
struct termios t0;
// 1B:
z_stream c_to_s;
z_stream s_to_c;
struct sockaddr_in serveraddr;
struct sockaddr_in clientaddr;
//struct hostent* server;
int sockfd;
int sockfd2; // new variable server.c
int log_fd;
int port_number;
int compress_flag = 0;
int log_flag = 0;
int port_flag = 0; // mandatory


/*---------error message helper function --------*/
void print_error(const char* msg){
	fprintf(stderr, "%s , error number: %d, strerror: %s\n ",msg, errno, strerror(errno));
    exit(1);
}

void reset(){
	if (tcsetattr(0, TCSANOW, &t0) < 0)
		print_error("Error: could not restore the terminal attributes");
}

void set(){
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

void write_with_check(int fd, const void *buf, size_t size){
	if(write(fd,buf,size) < 0){ 
		print_error("Error: writing error");
	} 
} 

ssize_t read_with_check(int fd, void *buf, size_t size)
{
	ssize_t s = read(fd,buf,size);
	if(s < 0){
		print_error(("Error: reading error"));
	}
	return s;
}
void close_exit() {
	check_close(p_to_c[1]);
	check_close(c_to_p[0]);
	check_close(sockfd);
	check_close(sockfd2);
}

/*-----------------------------[--shell]---------------------------------------*/
void shell_exit(){
	int status;
	waitpid(pid,&status,0);
	if (WIFEXITED(status)) { // something like CtrlD
		fprintf(stderr,"SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
		//exit(0); // dont need really
	}
	if (WIFSIGNALED(status)){ // send signal if CtrlC
	    fprintf(stderr,"SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
	    //exit(0);
	}
	/*
	check_close(p_to_c[1]);
	check_close(c_to_p[0]);
	check_close(sockfd);
	check_close(sockfd2);*/
	//check_close(sockfd);
	//check_close(sockfd2);
}

void make_pipe(int p[2]){
	if (pipe(p) < 0)
	{
		print_error("Error: creation of pipe(s)");
	}
}
void signal_handler(int sig){
	check_close(p_to_c[1]);
	check_close(c_to_p[0]);
	if(sig == SIGPIPE){
	  //kill(pid, SIGINT);
		shell_exit();
		exit(0); //dont need really
	}
}
void check_dup2(int old, int new){
	if(dup2(old,new) < 0){
		print_error("Error: dup2 error during I/O redirection");
	}
}
void check_close(int fd){
	if (close(fd) < 0){
		print_error("Error: close error during I/O redireciton");
	}
}
/*-----child and parent case (linux command /bin/bash exec vs regular input)*/
void child_case(){
	check_close(p_to_c[1]);
	check_close(c_to_p[0]);
	check_dup2(p_to_c[0], 0);
	check_dup2(c_to_p[1], 1);
	check_dup2(c_to_p[1], 2);
	check_close(p_to_c[0]);
	check_close(c_to_p[1]);
	char* pathname = "/bin/bash";
	char* args[2] = {pathname,NULL};
	if(execvp(pathname,args) == -1){
		print_error("Error: failed execute command");
	}
}

void parent_case() {
	check_close(p_to_c[0]);
	check_close(c_to_p[1]);
	short pevents =  POLLIN | POLLERR | POLLHUP;
	struct pollfd fds[2] = {
	//    int   fd;         /* file descriptor */
	//   short events;     /* requested events */
	//   short revents;    /* returned events */
		{sockfd2, pevents, 0},
		{c_to_p[0], pevents, 0}
	};
	char c;
	char buf[256];
	ssize_t s;
	while(1){
		int ret = poll(fds,2,0); // poll(fds,2,-1);
		if (ret < 0){
			print_error("Error: polling");
		}
		if (ret > 0){
			// returned events
			short socket_e = fds[0].revents;
			short shell_e = fds[1].revents;
			if(socket_e & POLLIN){
				s = read_with_check(sockfd2,buf,256);
				if(!compress_flag)
				{
					for(int i = 0; i < s ; i++)
					{
						c = buf[i];
						if (c == CTRLD || c == 0x04){
						   check_close(p_to_c[1]);
						   ssize_t input_shell = read_with_check(c_to_p[0], &buf,sizeof(char)*256);
							for (int i = 0; i < input_shell; i++){
								if(c == LF || c == crlf[1])
									write_with_check(sockfd2,&crlf,sizeof(char)*2);
								else
									write_with_check(sockfd2,&c, sizeof(char)*1);
							}
						   check_close(c_to_p[0]);
						   exit(0);
						} // if CTRLD
						else if (c == CTRLC || c == 0x03){
							kill(pid, SIGINT);
						} // if CTRLD
						else if (c == CR || c == LF || c == crlf[0] || c == crlf[1]){
							char lf = LF;
							write_with_check(p_to_c[1],&lf,sizeof(char)*1);
						} // if CR or LF
						else{
							write_with_check(p_to_c[1],&c,sizeof(char)*1);
						}
					}

				} // for
				if(compress_flag){
					int decompressed_bytes;
					unsigned char decompressed_buf[256];
					c_to_s.avail_in = s;
					c_to_s.next_in = (unsigned char *)buf;
					c_to_s.avail_out = 256;
					c_to_s.next_out = decompressed_buf;
					do {
						if (inflate(&c_to_s, Z_SYNC_FLUSH) != Z_OK) {
							print_error("Error: deflate error");
						}
					} while (c_to_s.avail_in > 0);
					decompressed_bytes = 256 - c_to_s.avail_out;
					//write_with_check(sockfd,decompressed_buf,decompressed_bytes);
					for (int i =0; i < decompressed_bytes; i++){
						c = decompressed_buf[i];
						if (c == CTRLD || c == 0x04){
						   check_close(p_to_c[1]);
						   ssize_t input_shell = read_with_check(c_to_p[0], &buf,sizeof(char)*256);
                                                        for (int i = 0; i < input_shell; i++){
                                                                if(c == LF || c == crlf[1])
                                                                        write_with_check(sockfd2,&crlf,sizeof(char)*2);
                                                                else
                                                                        write_with_check(sockfd2,&c, sizeof(char)*1);
                                                        }

						   check_close(c_to_p[0]);
						   exit(0);
						} // if CTRLD
						else if (c == CTRLC || c == 0x03){
							kill(pid, SIGINT);
						} // if CTRLD
						else if (c == CR || c == LF || c == crlf[0] || c == crlf[1]){
							char lf = LF;
							write_with_check(p_to_c[1],&lf,sizeof(char)*1);
						} // if CR or LF
						else{
							write_with_check(p_to_c[1],&c,sizeof(char)*1);
						}
					}
				}
			} // socket_e && POLLIN
			if (socket_e & (POLLERR | POLLHUP))
				exit(1);
			/*-- SHELL INPUT--*/
			if (shell_e & POLLIN){
				s = read(c_to_p[0],&buf,256);
				if (!compress_flag){
				  /*
				  for(int i = 0; i < s ; i++)
					{
						c = buf[i];
						if (c == CTRLD || c == 0x04){
						   check_close(p_to_c[1]);
						   check_close(c_to_p[0]);
						   exit(0);
						} // if CTRLD
						else if (c == CTRLC || c == 0x03){
							kill(pid, SIGINT);
						} // if CTRLD
						else if (c == CR || c == LF || c == crlf[0] || c == crlf[1]){
							write_with_check(sockfd2,&c,1);
						} // if CR or LF
						else{
							write_with_check(sockfd2,&c,1);
						}
						}*/
				  write_with_check(sockfd2,buf,s);
				  
				}
				if (compress_flag){
					int compressed_bytes;
					unsigned char compressed_buf[256];
					s_to_c.avail_in = s;
					s_to_c.next_in = (unsigned char *)buf;
					s_to_c.avail_out = 256;
					s_to_c.next_out = compressed_buf;
					do {
						if (deflate(&s_to_c, Z_SYNC_FLUSH) != Z_OK) {
							print_error("Error: deflate error");
						}
					} while (c_to_s.avail_in > 0);
					compressed_bytes = 256 - s_to_c.avail_out;
					write_with_check(sockfd2,compressed_buf,compressed_bytes);
				}
			}
			if(shell_e & (POLLHUP | POLLERR)){
				exit(0);
			}
		} // if ret > 0
	} // while(1)
	close_exit();

} // parent_case


// Socket
void socket_setup() {
	socklen_t client_size;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		print_error("Error: opening the socket");
	bzero((char*)&serveraddr, sizeof(serveraddr));
	// important
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons(port_number);
	if (bind(sockfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0){
		print_error("bind error");
	}
	if (listen(sockfd,5) < 0){
	  print_error("Error: listen error");
	}
	
	client_size = sizeof(clientaddr); // server = client address
	sockfd2 = accept(sockfd, (struct sockaddr *)&clientaddr, &client_size);
	if (sockfd2 < 0){
		print_error("accept error");
	}
}


// Compression // Flip inflate and deflate from client.c
void exit_compression() {
	deflateEnd(&s_to_c);
	inflateEnd(&c_to_s);
}
// https://zlib.net/zpipe.c
void compression_setup() {
	atexit(exit_compression);
	c_to_s.zalloc = Z_NULL;
	c_to_s.zfree = Z_NULL;
	c_to_s.opaque = Z_NULL;
	if (inflateInit(&c_to_s) != Z_OK) {
		print_error("Error: deflateInit error (allocate deflate state problem)");
	}
	s_to_c.zalloc = Z_NULL;
	s_to_c.zfree = Z_NULL;
	s_to_c.opaque = Z_NULL;
	if (deflateInit(&s_to_c, Z_DEFAULT_COMPRESSION) != Z_OK) {
		print_error("Error: inflateInit error (allocate inflate state problem)");
	}
}


/*----------------------main------------------------------*/
int main(int argc, char **argv){
	int choice;
	static struct option long_options[] = 
	{
		{"port", 1, 0, 'p'}, 
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
			case 'c':
				compress_flag = 1;
				break;
			case '?':
				fprintf(stderr, "unrecognized argument! Correct usage: ./lab1b-server --port=# --compress, where # is an integer greater than 1024\n");
				exit(1);
			default:
				fprintf(stderr, "unrecognized argument! Correct usage: ./lab1b-server --port=# --compress, where # is an integer greater than 1024\n");
				exit(1);
		}// switch
	}// while
	//set();
	if (port_flag != 1)
		print_error("Error: --port= is required! Specify a port number greater than 1024");
	if (port_number < 1024)
		print_error("Error: --port=port_number , Please choose a port number greater than 1024");
	if (compress_flag)
		compression_setup();
	socket_setup();
	make_pipe(c_to_p);
	make_pipe(p_to_c);
	atexit(shell_exit); // reassurance if signal handler doesn't work as intender 
	//atexit(close_exit);
	signal(SIGPIPE, signal_handler);
	//	signal(SIGINT, signal_handler);
	pid = fork(); // create new process => child
	if (pid == -1)
		print_error("fork() error");
	if (pid == 0){
		child_case();
	}
	else{
		parent_case();
	}
	exit(0);
}
