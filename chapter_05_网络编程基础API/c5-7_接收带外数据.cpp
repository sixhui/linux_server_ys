#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>
using namespace std;


#define BUF_SIZE 1024

int main(int argc, char const *argv[])
{
    assert(argc == 1);

    int port = 12345;
    int ip = INADDR_ANY;
    int backlog = 5;
    int res;

    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(ip);
    serv_addr.sin_port = htons(port);

    // socket
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(serv_sock >= 0);

    // bind
    res = bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    assert(res != -1);

    // listen
    res = listen(serv_sock, backlog);
    assert(res != -1);

    // accept
    socklen_t clnt_len;
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_len);
    assert(clnt_sock >= 0);

    // recv
    char buffer[BUF_SIZE];

    memset(buffer, '\0', BUF_SIZE);
    res = recv(clnt_sock, buffer, BUF_SIZE - 1, 0);
    printf("got %d bytes of normal data '%s'\n", res, buffer);

    memset(buffer, '\0', BUF_SIZE);
    res = recv(clnt_sock, buffer, BUF_SIZE - 1, MSG_OOB);
    printf("got %d bytes of oobmsg data '%s'\n", res, buffer);

    memset(buffer, '\0', BUF_SIZE);
    res = recv(clnt_sock, buffer, BUF_SIZE - 1, 0);
    printf("got %d bytes of normal data '%s'\n", res, buffer);

    close(clnt_sock);
    close(serv_sock);
    
    return 0;
}
