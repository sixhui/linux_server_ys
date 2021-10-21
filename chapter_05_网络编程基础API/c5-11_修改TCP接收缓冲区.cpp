#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <iostream>
using namespace std;

#define BUF_SIZE 1024

int main(int argc, char const *argv[])
{
    assert(argc == 1);

    int port = 12345;
    int recv_bufsize = 50;
    int backlog = 5;
    int serv_sock, clnt_sock;
    int res;

    struct sockaddr_in serv_addr, clnt_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // socket
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(serv_sock >= 0);
    setsockopt(serv_sock, SOL_SOCKET, SO_RCVBUF, &recv_bufsize, sizeof(recv_bufsize));
    int len;
    getsockopt(serv_sock, SOL_SOCKET, SO_RCVBUF, &recv_bufsize, (socklen_t*)&len);

    // bind
    res = bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(res != -1);

    // listen
    res = listen(serv_sock, backlog);
    assert(res != -1);

    // accept
    socklen_t clnt_addr_len;
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
    assert(clnt_sock != -1);

    // recv
    char buffer[BUF_SIZE];
    memset(buffer, '\0', BUF_SIZE);
    while(recv(clnt_sock, buffer, BUF_SIZE - 1, 0) > 0) {}

    // close
    close(clnt_sock);
    close(serv_sock);
    
    
    return 0;
}
