#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
using namespace std;

#define BUF_SIZE 4000

int main(int argc, char const *argv[])
{
    assert(argc == 1);

    const char* ip = "";
    int port = 12345;
    int send_bufsize = 2000;
    int sid;
    int res;
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    // socket
    sid = socket(PF_INET, SOCK_STREAM, 0);
    assert(sid >= 0);
    setsockopt(sid, SOL_SOCKET, SO_SNDBUF, &send_bufsize, sizeof(send_bufsize));
    int len;
    getsockopt(sid, SOL_SOCKET, SO_SNDBUF, &send_bufsize, (socklen_t*)&len);
    printf("the tcp send buffer size after setting is %d\n", send_bufsize);

    // connect
    res = connect(sid, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(res != -1);
    char buffer[BUF_SIZE];
    memset(buffer, 'a', BUF_SIZE);
    send(sid, buffer, BUF_SIZE, 0);

    // close
    close(sid);


    return 0;
}
