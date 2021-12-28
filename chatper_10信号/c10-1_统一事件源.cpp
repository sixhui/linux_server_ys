#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <fcntl.h>
#include <iostream>
using namespace std;

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

int setnonblocking(int fd);
void addfd(int epollfd, int fd);
void sig_handler(int sig);
void addsig(int sig);


int main(int argc, char const *argv[])
{
    assert(argc == 1);

    const char* ip = "";
    int port = 12345;
    int backlog = 5;
    int res = 0;

    int listenfd, epollfd;
    int n_fd_epoll;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);    

    // socket
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    
    // bind
    res = bind(listenfd, (sockaddr*)&addr, sizeof(addr));
    assert(res != -1);

    // listen
    res = listen(listenfd, backlog);
    assert(res != -1);

    // epoll
    epollfd = epoll_create(n_fd_epoll);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);
    epoll_event events[MAX_EVENT_NUMBER];

    // pipe
    res = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(res != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, listenfd);

    // signal
    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);
    bool stop_server = false;

    while(!stop_server){
        int n_already = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        assert(n_already >= 0);

        for(int i = 0; i < n_already; ++i){
            int sockfd = events[i].data.fd;
            // 新连接
            if(sockfd == listenfd){
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);
                int clnt_sock = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);

                addfd(epollfd, clnt_sock);
            }
            // 信号
            else if(sockfd == pipefd[0] && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                int n_sig = recv(pipefd[0], signals, sizeof(signals), 0);
                if(n_sig == -1){
                    continue;
                }
                else if(n_sig == 0){
                    continue;
                }
                else{
                    for(int i = 0; i < n_sig; ++i){
                        switch(signals[i]){
                            case SIGCHLD:
                            case SIGHUP: {
                                cout << "SIGHUP" << endl;
                                continue;
                            }
                            case SIGTERM: {

                            }
                            case SIGINT: {
                                cout << "SIGINT" << endl;
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            else{

            }
        }
    }



    
    cout << "close fds" << endl;
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    return 0;
}

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_op
}
void addfd(int epollfd, int fd);
void sig_handler(int sig);
void addsig(int sig);