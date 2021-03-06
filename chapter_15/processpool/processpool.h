#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/wait.h>

#define     oops(msg)       {perror(msg); exit(1);}

// 子进程类
class process{
public:
    process(): m_pid(-1) {}

public:
    pid_t   m_pid;
    int     m_pipefd[2];
};

// 进程池类，模板参数是处理逻辑任务的类
/**
 * @brief 进程池类，模板参数是处理逻辑任务的类
 * 注意：构造函数中，父进程使用 continue 继续循环，子进程使用 break 停止循环
 * 注意：进程对象的 m_pid 字段和进程池对象的 m_idx 字段不相同
 * 注意：为什么 sig_pipefd 需要在类外 static，类内 static 不可以吗 - 考虑 fork 子进程后管道不唯一
 * 注意：信号大小为 1B 正整数
 * 注意：为什么要统一事件源
 */
template<typename T>
class processpool{
private:
    processpool(int listenfd, int process_number = 8);
    
public:
    static processpool<T>* create(int listenfd, int process_number = 8){
        if(!m_instance){
            m_instance = new processpool<T>(listenfd, process_number);
        }
        return m_instance;
    }
    ~processpool(){
        delete [] m_sub_process;
    }

public:
    void run();                                         // 启动进程池

private:
    void setup_sig_pipe();                              // 统一事件源
    void run_parent();
    void run_child();

private:
    static processpool<T>*      m_instance;             // 进程池实例
    process*                    m_sub_process;          // 进程池维护的资源 - 子进程数组

    int                         m_process_n;            // 子进程数量
    int                         m_idx;                  // 子进程编号
    int                         m_stop;                 // 子进程停止判断标志
    int                         m_listenfd;             // 监听套接字
    int                         m_epollfd;

    static const int            MAX_PROCESS_N   = 16;   // 进程池允许的最大子进程数量
    static const int            USER_PER_PROCESS= 65526;// 每个子进程最大处理的用户数
    static const int            MAX_EVENT_N     = 10000;// epoll 最多能处理的事件数

};

static int                      sig_pipefd[2];          // 信号管道，用于统一事件源 - 信号处理时发送信号到管道

static int setnonblock(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblock(fd);
}

static void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*) &msg, 1, 0);
    errno = save_errno;
}

static void addsig(int sig, void(handler)(int), bool restart=true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));

    sa.sa_handler = handler;
    if(restart) sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);

    if((sigaction(sig, &sa, NULL)) == -1) oops("fail sigaction()");
}


template<typename T>
processpool<T>* processpool<T>::m_instance = NULL;

/* 构造函数 */
template<typename T>
processpool<T>::processpool(int listenfd, int process_number): 
m_listenfd(listenfd), m_process_n(process_number), m_idx(-1), m_stop(false){
    assert((process_number > 0) && (process_number <= MAX_PROCESS_N));

    if((m_sub_process = new process[process_number]) == NULL) oops("fail new process[]");
    // 创建 process_number 个子进程，并建立它们和父进程之间的管道
    for(int i = 0; i < process_number; ++i){
        if(socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd) == -1) oops("fail socketpair");
        
        if((m_sub_process[i].m_pid = fork()) < 0) oops("fail fork");
        if(m_sub_process[i].m_pid > 0){                 // 父进程
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else{                                           //子进程
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

/* 统一事件源 - 往 sigpipefd[1] 写 */
template<typename T>
void processpool<T>::setup_sig_pipe(){
    // 创建事件监听 epoll
    if((m_epollfd = epoll_create(5)) == -1) oops("fail epoll_create");

    // 创建信号管道
    if(socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd) == -1) oops("fail socketpair");

    // 监听信号管道
    setnonblock(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);

    // 设置信号处理函数
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT,  sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

template<typename T>
void processpool<T>::run(){
    if(m_idx != -1){
        run_child();
    }
    else{
        run_parent();
    }
}

/**
 * @brief 子进程负责处理业务
 * 每个子进程有两个管道：信号管道 + 与父进程通信管道
 */
template<typename T>
void processpool<T>::run_child(){
    epoll_event events[MAX_EVENT_N];
    T*          users;
    int         pipefd;
    int         event_n;
    int         n_read;

    // 初始化 epollfd！！！ 父子进程的 m_epollfd 不是同一个，是各自创建的 - 把 epollfd 创建过程提取出来比较好
    setup_sig_pipe();

    pipefd = m_sub_process[m_idx].m_pipefd[1]; // 父进程用 0，子进程用 1
    addfd(m_epollfd, pipefd);

    if((users = new T[USER_PER_PROCESS]) == NULL) oops("fail new T[]");

    while(!m_stop){
        // 监听
        if((event_n = epoll_wait(m_epollfd, events, MAX_EVENT_N, -1)) < 0 && (errno != EINTR)){
            printf("fail epoll_wait\n");
            break;
        }
        printf("child %d receive epoll event\n", m_idx);
        // 处理
        for(int i = 0; i < event_n; ++i){
            int sockfd = events[i].data.fd;
            if((sockfd == pipefd) && (events[i].events & EPOLLIN)){                 // 父子进程管道读取 - 客户连接请求
                int client = 0; // 可删？
                n_read = recv(sockfd, (char*)&client, sizeof(client), 0);
                if(((n_read < 0) && (errno != EAGAIN)) || n_read == 0){
                    continue;
                }
                
                // 获取客户端连接
                struct sockaddr_in  clnt_addr;
                socklen_t           clnt_addr_len;
                int connfd  = accept(m_listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
                if(connfd < 0) {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                // 监听客户端连接
                addfd(m_epollfd, connfd);
                // 初始化处理器
                users[connfd].init(m_epollfd, connfd, clnt_addr);
            }
            else if((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)){     // 信号
                int sig;
                char signals[1024];
                n_read = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(n_read <= 0){
                    continue;
                }
                else{
                    for(int i = 0; i < n_read; ++i){
                        switch(signals[i]){
                            case SIGCHLD:{
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0){ // 子进程，有必要吗
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:{
                                m_stop = true;
                                break;
                            }
                            default: break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN){                                    // 客户数据请求 - 调用逻辑处理对象
                users[sockfd].process();
            }
            else{
                continue;
            }
        }
    }

    delete[] users;
    users = NULL;
    close(pipefd);
    // close(m_listenfd); // 谁创建，谁关闭
    close(m_epollfd);
}

/**
 * @brief 父进程，主要分配任务
 * 
 */
template<typename T>
void processpool<T>::run_parent(){
    epoll_event events[MAX_EVENT_N];
    int event_n;
    int n_read = -1;
    int sub_process_counter = 0;
    int new_conn = 1;

    // 初始化 epollfd！！！ 父子进程的 m_epollfd 不是同一个，是各自创建的 - 把 epollfd 创建过程提取出来比较好
    setup_sig_pipe();

    addfd(m_epollfd, m_listenfd);

    while(!m_stop){
        // 监听
        if((event_n = epoll_wait(m_epollfd, events, MAX_EVENT_N, -1)) < 0 && (errno != EINTR)){
            printf("fail epoll_wait\n");
            break;
        }

        // 处理
        for(int i = 0; i < event_n; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd){                                           // 客户连接请求 - 为新连接分配一个子进程
                int i = sub_process_counter;
                do{
                    if(m_sub_process[i].m_pid != -1) break;
                    i = (i + 1) % m_process_n; // 改为 m_sub_process_n
                }while(i != sub_process_counter);

                if(m_sub_process[i].m_pid == -1){
                    m_stop = true;
                    break;
                }

                sub_process_counter = (i + 1) % m_process_n;
                // 通知子进程 - 不是用信号！！！
                int res = send(m_sub_process[i].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                printf("%d\n", errno);
                printf("send request to child %d - res %d\n", i, res);
            }
            else if((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)){ // 信号
                int sig;
                char signals[1024];
                n_read = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(n_read <= 0) continue;

                for(int i = 0; i < n_read; ++i){
                    switch(signals[i]){
                        case SIGCHLD:{
                            pid_t pid;
                            int stat;
                            while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                // 进程池中pid子进程退出，则主进程需要清理相应管道，并设置相应m_pid为-1，标记退出
                                for(int i = 0; i < m_process_n; ++i){
                                    if(m_sub_process[i].m_pid == pid){
                                        printf("child %d join\n", i);
                                        close(m_sub_process[i].m_pipefd[0]);
                                        m_sub_process[i].m_pid = -1;
                                    }
                                }
                            }
                            // 检查是否全部子进程都退出了，是则主进程也退出
                            m_stop = true;
                            for(int i = 0; i < m_process_n; ++i){
                                if(m_sub_process[i].m_pid != -1) m_stop = false;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:{   // 父进程收到终止信号，则杀死所有子进程
                            printf("kill all the child now\n");
                            for(int i = 0; i < m_process_n; ++i){
                                if(m_sub_process[i].m_pid != -1) kill(m_sub_process[i].m_pid, SIGTERM);
                            }
                        }
                        default: break;
                    }
                }
            }
            else{
                continue;
            }
        }
    }

    // close(m_listenfd);
    close(m_epollfd);
}


#endif