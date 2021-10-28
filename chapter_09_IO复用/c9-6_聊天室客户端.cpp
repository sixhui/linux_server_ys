#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <sys/poll.h>
#include <fcntl.h>
using namespace std;

#define _GNU_SOURCE 1
#define BUFFER_SIZE 64


int main(int argc, char const *argv[])
{
    assert(argc == 1);

    const char* ip = "47.103.53.49";
    int port = 12345;
    int res;
    int serv_sock;
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    // socket
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(serv_sock >= 0);
    
    // connect
    res = connect(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    assert(res != -1);

    // poll
    pollfd fds[2];
    /* 监听标准输入 */
    fds[0].fd = 0; 
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    /* 监听 serv_sock */
    fds[1].fd = serv_sock;
    fds[1].events = POLLIN | POLLRDHUP; // POLLRDHUP 连接关闭事件
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    res = pipe(pipefd);
    assert(res != -1);

    while(1){
        res = poll(fds, 2, -1);
        if(res < 0){
            cout << "error" << endl;
            break;
        }

        if(fds[1].revents & POLLRDHUP){
            cout << "close by server" << endl;
            break;
        }
        else if(fds[1].revents & POLLIN){
            memset(read_buf, '\0', BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE - 1, 0);
            cout << "get content: " << read_buf << endl;
        }

        if(fds[0].revents & POLLIN){
            /* splice: stdin -> sockfd 零拷贝 */
            res = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            res = splice(pipefd[0], NULL, serv_sock, NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
        }
    }

    close(serv_sock);
    return 0;
}
