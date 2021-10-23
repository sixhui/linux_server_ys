#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sys/epoll.h>
#include <fcntl.h>
using namespace std;

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

/* 将文件描述符设置为非阻塞 */
int setnonblocking(int fd);
void addfd(int epollfd, int sfd, bool enable_et);
void lt(epoll_event* events, int number, int epollfd, int listenfd);
void et(epoll_event* events, int number, int epollfd, int listenfd);

int main(int argc, char const *argv[])
{
    assert(argc == 1);

    int port = 12345;
    int listenfd, backlog = 5;
    int epollfd, sfd_num = 5;
    int res;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // socket
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // bind
    res = bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    assert(res != -1);

    // listen
    res = listen(listenfd, backlog);
    assert(res != -1);

    // epoll
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(sfd_num);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, true); /* 将监听 socket 添加到 epollfd，并启用 ET 模式*/

    while(1){
        res = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        assert(res >= 0);

        // lt(events, res, epollfd, listenfd);
        et(events, res, epollfd, listenfd);
    }
    
    close(listenfd);
    return 0;
}

void lt(epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; ++i){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){ // 接收连接 socket，并添加到 epollfd 进行监听
            struct sockaddr_in clnt_addr;
            socklen_t clnt_addr_len = sizeof(clnt_addr);
            int clnt_sock = accept(listenfd, (struct sockaddr*) &clnt_addr, &clnt_addr_len);

            /* LT 方式监听客户端 socket */
            addfd(epollfd, clnt_sock, false);
        }
        else if(events[i].events & EPOLLIN){ // 发生读就绪事件，接收数据
            /* 只要 socket 读缓存还有未读出的数据就会触发 */
            cout << "EPOLLIN LT trigger once" << endl;
            memset(buf, '\0', BUFFER_SIZE);
            int res = recv(sockfd, buf, BUFFER_SIZE - 1, 0);

            if(res <= 0) {
                cout << "close" << endl;
                close(sockfd); 
                continue;
            }
            cout << "GET " << res << " bytes of content: " << buf << endl;
        }
        else cout << "something else happened" << endl;
    }
}

void et(epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; ++i){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            struct sockaddr_in clnt_addr;
            socklen_t clnt_addr_len = sizeof(clnt_addr);
            int clnt_sock = accept(sockfd, (struct sockaddr*) &clnt_addr, &clnt_addr_len);
            
            addfd(epollfd, clnt_sock, true);
        }
        else if(events[i].events & EPOLLIN){
            /* 不会重复触发，所以需要循环确保数据全部读出 */
            cout << "EPOLLIN ET trigger once" << endl;
            while(1){
                memset(buf, '\0', BUFFER_SIZE);
                int res = recv(sockfd, buf, BUFFER_SIZE - 1, 0);                    // 轮询非阻塞 fd
                cout << "recv ->" << res << endl;
                if(res == 0){                                                       // 对方关闭连接
                    cout << "closed by client" << endl;                             
                    close(sockfd);
                    break;
                }
                else if(res < 0 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))){   // 读就绪未发生 - 缓冲区低水位，自动重置 EPOLLIN-ET 事件
                    cout << "read later" << endl;
                    break;
                }
                else if(res < 0){                                                   // 出错
                    cout << "close for error" << endl;
                    break;
                }
                else {                                                              // 读就绪发生  - 收到数据
                    cout << "get content：" << buf << endl;
                }
            }
        }
        else cout << "something else happened" << endl;
    }
}


int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    // fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool enable_et){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(enable_et) event.events |= EPOLLET;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
