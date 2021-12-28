#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#define     oops(msg)     {perror(msg); exit(1); }

int port        = 12345;
int backlog     = 3;

int main(int argc, char const *argv[])
{
    int         sockfd, clnt_fd;
    struct      sockaddr_in addr, clnt_addr;
    socklen_t   clnt_addr_len;
    char        remote[INET_ADDRSTRLEN];

    // socket
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) oops("fail socket");

    // bind
    bzero(&addr, sizeof(addr));
    addr.sin_family         = AF_INET;
    addr.sin_addr.s_addr    = htonl(INADDR_ANY);
    addr.sin_port           = htons(port);
    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) oops("fail bind");

    // listen
    printf("before listen\n");
    if(listen(sockfd, backlog) == -1) oops("fail listen");
    printf("after listen\n");

    sleep(30);

    // accept
    clnt_addr_len = sizeof(clnt_addr);
    if((clnt_fd = accept(sockfd, (struct sockaddr*) &clnt_addr, &clnt_addr_len)) == -1) oops("fail accept");

    printf("connected with ip: %s and port: %d\n", inet_ntop(AF_INET, &clnt_addr.sin_addr, remote, INET_ADDRSTRLEN), ntohs(clnt_addr.sin_port));

    close(clnt_fd);
    close(sockfd);




    
    
    return 0;
}
