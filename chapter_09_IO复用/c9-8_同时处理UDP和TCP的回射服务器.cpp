#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/epoll.h>
#include <fcntl.h>
using namespace std;

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

int setnonblock(int fd);
void addfd(int epollfd, int fd);

int main(int argc, char const *argv[])
{
    assert(argc == 1);

    const char* ip = "";
    int port = 12345;
    int res;
    int listenfd;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    // TCP
    // socket
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // bind
    res = bind(listenfd, (struct sockaddr*) &addr, sizeof(addr));
    assert(res != -1);

    // listen
    res = listen(listenfd, 5);
    assert(res != -1);

    // UDP
    // socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int udpfd = socket(PF_INET, SOCK_DGRAM, 0);
    assert(udpfd >= 0);

    // bind
    res = bind(udpfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(res != -1);

    // EPOLL
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    /* 注册监听事件 */
    addfd(epollfd, listenfd);
    addfd(epollfd, udpfd);

    // epoll_wait
    epoll_event events_res[MAX_EVENT_NUMBER];
    while(1){
        int n_event = epoll_wait(epollfd, events_res, MAX_EVENT_NUMBER, -1);
        assert(n_event >= 0);

        for(int i = 0; i < n_event; ++i){
            int sockfd = events_res[i].data.fd;
            if(sockfd == listenfd){
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);
                int tcpfd = accept(listenfd, (struct sockaddr*) &clnt_addr, &clnt_addr_len);
                cout << "tcp accept " << tcpfd << endl;

                addfd(epollfd, tcpfd);
            }
            else if(sockfd == udpfd){
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);

                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);

                res = recvfrom(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr*) &clnt_addr, &clnt_addr_len);
                cout << "udp recieve " << res << "bytes" << endl;
                if(res > 0){
                    sendto(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr*) &clnt_addr, clnt_addr_len);
                    cout << "udp echo: " << buf << endl;
                }
            }
            else if(events_res[i].events & EPOLLIN){
                char buf[TCP_BUFFER_SIZE];
                while(1){
                    memset(buf, '\0', TCP_BUFFER_SIZE);

                    res = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
                    cout << "tcp receive " << res << " bytes" << endl;
                    if(res < 0){
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            cout << "errno == EAGAIN" << endl;
                            break;
                        }
                        cout << "tcp close for error: " << sockfd << endl;
                        close(sockfd);
                        break;
                    }
                    else if(res == 0){
                        cout << "tcp closed by client" << endl;
                        close(sockfd);
                    }
                    else{
                        cout << "tcp echo: " << buf << endl;
                        send(sockfd, buf, res, 0);
                    }
                }
            }
            else{
                cout << "something else happened" << endl;
            }
        }
    }



    close(listenfd);
    return 0;
}

int setnonblock(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    epoll_ctl(epollfd, EPOLL_CTL_ADD,fd, &event);
    setnonblock(fd);
}