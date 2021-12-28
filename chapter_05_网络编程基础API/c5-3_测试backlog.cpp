#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>

#define     oops(msg)   {perror(msg); exit(1);}

int port    = 12345;
int backlog = 1;
bool stop   = false;

void handle_int(int sig){
    printf("SIGINT\n");
    stop = true;
}

int main(int argc, char const *argv[])
{
    int     sockfd, clnt_fd;
    struct  sockaddr_in addr, clnt_addr;
    socklen_t clnt_addr_len;

    // signal
    signal(SIGINT, handle_int);

    // socket
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) oops("fail socket");

    // bind
    bzero(&addr, sizeof(addr));
    addr.sin_family     = AF_INET;
    addr.sin_addr.s_addr= htonl(INADDR_ANY);
    addr.sin_port       = htons(port);
    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) oops("fail bind");
    
    // listen
    if(listen(sockfd, backlog) == -1) oops("fail listen");

    while(!stop){
        sleep(1);
    }
    printf("%d\n", stop);
    
    
    return 0;
}

