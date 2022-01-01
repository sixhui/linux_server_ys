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

#define     MAX_EVENT_N         1024
#define     MAX_LISTENED_SOCK_N 5
#define     BUFFER_SIZE         10
#define     oops(msg)           {perror(msg); exit(1);}

int     setnonblocking(int fd);
void    addfd(int epollfd, int sfd, bool enable_et);
void    lt(epoll_event* events, int number, int epollfd, int listenfd);
void    et(epoll_event* events, int number, int epollfd, int listenfd);

int main(int argc, char const *argv[])
{
    int ip      = INADDR_ANY;
    int port    = 12345;
    int listenfd, backlog = 5;
    int epollfd;
    epoll_event events[MAX_EVENT_N];    // 接收 socket 就绪事件
    int event_n;

    // socket
    if((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) oops("fail socket");

    // bind
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(ip);
    serv_addr.sin_port          = htons(port);

    if((bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))) == -1) oops("fail bind");

    // listen
    if((listen(listenfd, backlog)) == -1) oops("fail listen");

    // epoll
    if((epollfd = epoll_create(MAX_LISTENED_SOCK_N)) == -1) oops("fail epoll_create");
    addfd(epollfd, listenfd, true);     // 将 listenfd 添加到 epollfd，并启用 ET 模式

    while(1){
        if((event_n = epoll_wait(epollfd, events, MAX_EVENT_N, -1)) < 0) oops("fail epoll_wait");

        // lt(events, event_n, epollfd, listenfd);
        et(events, event_n, epollfd, listenfd);
    }
    
    close(listenfd);
    return 0;
}

/* !!!只要 socket 读缓存还有未读出的数据就会触发 */
void lt(epoll_event* events, int number, int epollfd, int listenfd){
    char    buf[BUFFER_SIZE];
    int     sockfd;
    int     n_read;

    for(int i = 0; i < number; ++i){
        sockfd = events[i].data.fd;
        if(sockfd == listenfd){             // listenfd 的事件：接受 clnt_sock，并添加到 epollfd 进行监听
            int                 clnt_sock;
            struct sockaddr_in  clnt_addr;
            socklen_t           clnt_addr_len = sizeof(clnt_addr);

            if((clnt_sock = accept(listenfd, 0, 0)) == -1) {
                oops("fail accept == -1");
            }
            else{
                addfd(epollfd, clnt_sock, false);
            }
        }
        else if(events[i].events & EPOLLIN){ // 读就绪事件：接收数据
            cout << "EPOLLIN LT trigger once" << endl;

            memset(buf, '\0', BUFFER_SIZE);
            if((n_read = recv(sockfd, buf, BUFFER_SIZE - 1, 0)) <= 0){
                cout << "close" << sockfd << endl;
                close(sockfd);
            }
            else{
                cout << "GET " << n_read << " bytes of content: " << buf << endl;
            }
        }
        else{
            cout << "something else happened" << endl;
        }
    }
}

/* !!!不会重复触发，所以需要 while 确保数据全部读出 */
void et(epoll_event* events, int number, int epollfd, int listenfd){
    char    buf[BUFFER_SIZE];
    int     sockfd;
    int     n_read;
    
    for(int i = 0; i < number; ++i){
        sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            int                 clnt_sock;
            struct sockaddr_in  clnt_addr;
            socklen_t           clnt_addr_len = sizeof(clnt_addr);

            if((clnt_sock = accept(sockfd, 0, 0)) == -1) {
                oops("fail accept == -1")
            }
            else{
                addfd(epollfd, clnt_sock, true);
            }
        }
        else if(events[i].events & EPOLLIN){
            cout << "EPOLLIN ET trigger once" << endl;

            while(1){
                memset(buf, '\0', BUFFER_SIZE);
                n_read = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
                cout << "n read " << n_read << endl;
                if(n_read > 0){
                    cout << "READ ALREADY: " << buf << endl;               
                }
                else if(n_read == 0){
                    cout << "CLOSE by CLNT" << endl;                             
                    close(sockfd);
                    break;
                }
                else if(n_read < 0 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))){   // 数据读取完毕，自动重置 EPOLLIN-ET 事件
                    cout << "READ LATER" << endl;
                    break;
                }
                else if(n_read < 0){       
                    cout << "CLOSE for ERROR" << endl;
                    break;
                }
            }

            cout << "EPOLLIN ET exist" << endl;
        }
        else{
            cout << "something else happened" << endl;
        }
    }
}


int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
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
