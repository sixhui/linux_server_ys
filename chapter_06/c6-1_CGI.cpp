#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

#define     BACKLOG     3
#define     oops(msg)   {perror(msg); exit(1);}

/**
 * @brief dup2(connfd, 1) 实现 CGI
 * 注意：dup2(connfd, 1) == close + dup
 */
int main(int argc, char const *argv[])
{
    int         ip      = INADDR_ANY;
    int         port    = 12345;    
    int         listenfd, connfd;
    sockaddr_in serv_addr;
    
    // socket
    if((listenfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) oops("fail socket")
    
    // bind
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(ip);
    serv_addr.sin_port          = htons(port);

    if(bind(listenfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) oops("fail bind");

    // listen
    if(listen(listenfd, BACKLOG) == -1) oops("fail listen");

    // accept
    if((connfd = accept(listenfd, 0, 0)) == -1) oops("fail accept");

    // close(STDOUT_FILENO);
    // dup(connfd);
    dup2(connfd, 1);
    printf("abcd\n");



    close(connfd);
    close(listenfd);
    return 0;
}




