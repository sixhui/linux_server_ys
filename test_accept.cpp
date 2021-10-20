#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>

int main(int argc, char const *argv[])
{
    if(argc <= 1) {
        printf("usage: %s port\n", basename(argv[0]));
        exit(1);
    }

    int port = atoi(argv[1]);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    // socket
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock != -1);

    // bind
    int res = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(res != -1);
    
    // listen
    res = listen(sock, 5);
    assert(res != -1);

    /* 暂停 20s，让客户端连接掉线 */
    sleep(20);
    printf("end sleep %d\n", port);

    // accept
    struct sockaddr_in client;
    socklen_t client_addrlen = sizeof(client);
    int sock_res = accept(sock, (struct sockaddr*)&client, &client_addrlen);
    char remote[INET_ADDRSTRLEN];
    printf("connected with ip: %s and port: %d\n", 
        inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN), 
        ntohs(client.sin_port));
    close(sock_res);
    
    return 0;
}
