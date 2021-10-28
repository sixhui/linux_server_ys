#define _GNU_SOURCE 1
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <sys/poll.h>
#include <fcntl.h>
using namespace std;

#define USER_LIMIT 5        /* 最大用户数 */
#define BUFFER_SIZE 64      /* 读缓冲区大小 */
#define FD_LIMIT 65535      /* 文件描述符限制 */

/* 客户数据：客户端地址、待写入客户端数据、从客户端读取的数据 */
/* "socket - 客户数据"关联的方法：足够大的客户数据数组，使用 socket 的值做索引 */
struct client_data{
    sockaddr_in addr;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

int setnonblocking(int sfd);

int main(int argc, char const *argv[])
{
    assert(argc == 1);

    int port = 12345;
    int res;
    int listenfd, backlog = 5;

    struct sockaddr_in serv_addr, clnt_addr;
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
    
    // poll
    client_data* users = new client_data[FD_LIMIT];
    pollfd fds[USER_LIMIT + 1]; /* 1个监听socket，USER_LIMIT个连接socket */
    int user_counter = 0;
    for(int i = 1; i <= USER_LIMIT; ++i){
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while(1){
        res = poll(fds, user_counter + 1, -1);
        assert(res > 0);

        for(int i = 0; i < user_counter + 1; ++i){
            if(fds[i].fd == listenfd && (fds[i].revents & POLLIN)){
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);
                int clnt_sock = accept(listenfd, (struct sockaddr*) &clnt_addr, &clnt_addr_len);
                assert(clnt_sock >= 0);

                /* 如果请求太多，则关闭新到的连接 ... */

                ++user_counter;
                users[clnt_sock].addr = clnt_addr;
                setnonblocking(clnt_sock);
                fds[user_counter].fd = clnt_sock;
                fds[user_counter].events = POLLIN | POLLHUP | POLLERR;
                fds[user_counter].revents = 0;

                cout << "comes a new user, now have " << user_counter << " users" << endl;
            }
            else if(fds[i].revents & POLLERR){
                cout << "get an error from " << fds[i].fd << endl;
                continue;
            }
            else if(fds[i].revents & POLLRDHUP){
                /* 如果客户端关闭连接，则服务器也关闭连接，并将用户数减 1 */
                close(fds[i].fd);
                users[fds[i].fd] = users[fds[user_counter].fd];
                fds[i] = fds[user_counter];
                --i;
                --user_counter;
                cout << "a client left\n" << endl;
            }
            else if(fds[i].revents & POLLIN){
                int connfd = fds[i].fd;
                memset(users[connfd].buf, '\0', BUFFER_SIZE);
                res = recv(connfd, users[connfd].buf, BUFFER_SIZE - 1, 0);
                cout << "get " << res << " bytes of client data " << users[connfd].buf << " from " << connfd << endl;
                if(res < 0){
                    /* 读出错，则关闭连接 */
                    if(errno != EAGAIN){
                        close(fds[i].fd);
                        users[fds[i].fd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        --i;
                        --user_counter;
                    }
                }
                else if(res == 0){

                }
                else{
                    /* 收到数据，通知其他 socket 连接准备写数据 */
                    for(int j = 1; j <= user_counter; ++j){
                        if(fds[j].fd == connfd) continue;

                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
            }
            else if(fds[i].revents & POLLOUT){
                int connfd = fds[i].fd;
                if(!users[connfd].write_buf) continue;

                res = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = nullptr;
                
                // 重新注册可读事件
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;

            }
        }
    }

    return 0;
}


int setnonblocking(int sfd){
    int old_option = fcntl(sfd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(sfd, F_SETFL, new_option);
    return old_option;
}