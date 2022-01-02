#include <arpa/inet.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <assert.h>
using namespace std;

#define     MAX_EVENT_N         1024
#define     MAX_LISTENED_SOCK_N 5
#define     BUFFER_SIZE         1024
#define     BACKLOG             5
#define     oops(msg)           {perror(msg); exit(1);}
struct fds{
    int epollfd;
    int sockfd;
};

int     setnonblocking(int sockfd);
void    reset_oneshot(int epollfd, int sockfd);
void    addfd(int epollfd, int sockfd, bool oneshot);
void*   worker(void* arg);

int main(int argc, char const *argv[])
{
    int         ip      = INADDR_ANY;
    int         port    = 12345;
    int         listenfd;
    int         epollfd, event_n;
    sockaddr_in serv_addr, clnt_addr;
    epoll_event events[MAX_EVENT_N];    // 接收 epoll_wait 反馈

    // socket
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // bind
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(ip);
    serv_addr.sin_port          = htons(port);

    if((bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1) oops("fail bind");

    // listen
    if(listen(listenfd, BACKLOG) == -1) oops("fail listen");

    // epoll
    if((epollfd = epoll_create(MAX_LISTENED_SOCK_N)) == -1) oops("fail epoll_create");
    addfd(epollfd, listenfd, false);    // /* listenfd 上不能注册 EPOLLONESHOT 事件，否则后续客户端连接将不再触发 EPOLLIIN 事件 */

    while(1){
        /* 在此阻塞 */
        cout << "epoll wait pre" << endl;
        if((event_n = epoll_wait(epollfd, events, MAX_EVENT_N, -1)) == -1) oops("fail epoll_wait");
        cout << "epoll wait after" << endl;

        for(int i = 0; i < event_n; ++i){
            int sfd = events[i].data.fd;
            if(sfd == listenfd){                    // 监听套接字
                int clnt_sock;

                if((clnt_sock = accept(listenfd, 0, 0)) == -1){
                    oops("fail accept");
                }
                else{
                    addfd(epollfd, clnt_sock, true); /* 客户端 socket 上注册 EPOLLONESHOT 事件 */
                }
            }
            else if(events[i].events & EPOLLIN){    // 连接套接字
                pthread_t thread;

                fds fds_for_new_worker;
                fds_for_new_worker.epollfd  = epollfd;
                fds_for_new_worker.sockfd   = sfd;

                /* 启动一个工作线程为 sfd 服务 */
                pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
            }
            else cout << "something else happened" << endl;
        }
    }

    close(listenfd);
    return 0;
}

void addfd(int epollfd, int sfd, bool oneshot){
    epoll_event event;
    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET;
    if(oneshot) event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &event);
    setnonblocking(sfd);
}

void* worker(void* arg){
    int epollfd = ((fds*)arg)->epollfd;
    int sfd = ((fds*)arg)->sockfd;
    cout << "start new thread to receive data on fd: " << sfd << endl;

    char buf[BUFFER_SIZE];
    while(1){
        memset(buf, '\0', BUFFER_SIZE);
        int res = recv(sfd, buf, BUFFER_SIZE - 1, 0);   // 轮询非阻塞 fd
        if(res == 0){                                   // 对方关闭连接
            close(sfd);
            cout << "close" << endl;
            break;
        }
        else if(res < 0 && errno == EAGAIN){            // 读就绪未发生 - 重置 EPOLLONESHOT 事件
            reset_oneshot(epollfd, sfd);
            cout << "read later" << endl;
            break;
        }
        else if(res < 0){
            cout << "close for error" << endl;          // 出错
            break;
        }
        else{                                           // 读就绪发生  - 收到数据
            cout << "get content: " << buf << endl;
            /* 休眠 5s，模拟数据处理过程 */
            sleep(5);
        }
    }
    cout << "end thread receiving data on fd: " << sfd << endl;
    return NULL;
}

void reset_oneshot(int epollfd, int sfd){
    epoll_event event;
    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    /* !!! MOD */
    epoll_ctl(epollfd, EPOLL_CTL_MOD, sfd, &event);
}

int setnonblocking(int sfd){
    int old_option = fcntl(sfd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(sfd, F_SETFL, new_option);
    return old_option;
}