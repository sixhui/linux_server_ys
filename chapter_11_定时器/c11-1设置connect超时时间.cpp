#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <iostream>
using namespace std;

/* 连接超时函数 */
int timeout_connect(const char* ip, int port, int time);

int main(int argc, char const *argv[])
{
    assert(argc == 1);

    const char* ip = "47.103.53.49";
    int port = 12345;

    int sid = timeout_connect(ip, port, 10);
    if(sid < 0) return 1;

    return 0;
}

int timeout_connect(const char* ip, int port, int time){
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    int res;

    // socket
    int sid = socket(PF_INET, SOCK_STREAM, 0);
    assert(sid >= 0);
    struct timeval timeout;
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    socklen_t len = sizeof(timeout);
    res = setsockopt(sid, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    assert(res != -1);

    // connect
    res = connect(sid, (struct sockaddr*)&addr, sizeof(addr));
    if(res == -1){
        /* 超时 */
        if(errno == EINPROGRESS){
            cout << "timeout - connecting, process timeout logic" << endl;
            return -1;
        }
        cout << "error occur when connecting to server" << endl;
        return -1;
    }
    return sid;
}

