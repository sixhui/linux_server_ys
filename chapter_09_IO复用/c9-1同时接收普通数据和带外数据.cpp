#include <stdlib.h>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <assert.h>
#include <unistd.h>
using namespace std;


int main(int argc, char const *argv[])
{
    assert(argc != 1);

    const char* ip = "";
    int port = 12345;
    int res;

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    // socket
    int sid = socket(PF_INET, SOCK_STREAM, 0);
    assert(sid >= 0);

    // bind
    res = bind(sid, (struct sockaddr*) &addr, sizeof(addr));
    assert(res != -1);

    // listen
    res = listen(sid, 5);
    assert(res != -1);

    // accept
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len;
    int connfd = accept(sid, (struct sockaddr*) &clnt_addr, &clnt_addr_len);
    assert(connfd >= 0);

    // select
    char buf[1024];
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);
    while(1){
        memset(buf, '\0', sizeof(buf));

        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);
        res = select(connfd + 1, &read_fds, NULL, &exception_fds, NULL);
        assert(res != -1);

        /* 可读事件，普通 recv() 读取数据 */
        if(FD_ISSET(connfd, &read_fds)){
            res = recv(connfd, buf, sizeof(buf) - 1, 0);
            if(res <= 0) break;
            printf("get %d bytes of normal data: %s\n", res, buf);
        }
        /* 异常事件，带 MSG_OOB 标志的 recv() 读取带外数据 */
        else if(FD_ISSET(connfd, &exception_fds)){
            res = recv(connfd, buf, sizeof(buf) - 1, MSG_OOB);
            if(res <= 0) break;
            printf("get %d bytes of oob data: %s\n", res, buf);
        }
    }

    close(connfd);
    close(sid);

    return 0;
}
