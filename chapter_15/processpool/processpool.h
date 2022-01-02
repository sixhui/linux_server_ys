#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

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
    void setup_sig_pipe();
    void run_parent();
    void run_child();

private:
    static processpool<T>*      m_instance;             // 进程池实例
    process*                    m_sub_process;          // 进程池维护的资源 - 子进程数组

    int                         m_process_n;            // 子进程数量
    int                         m_idx;                  // 子进程编号
    int                         m_stop;                 // 子进程停止判断标志
    int                         m_listenfd;             // 监听套接字

    static const int            MAX_PROCESS_N   = 16;   // 进程池允许的最大子进程数量
    static const int            USER_PER_PROCESS= 65526;// 每个子进程最大处理的用户数
    static const int            MAX_EVENT_N     = 10000;// epoll 最多能处理的事件数

};

template<typename T>
processpool<T>* processpool<T>::m_instance = NULL;

template<typename T>
processpool<T>::processpool(int listenfd, int process_number): 
m_listenfd(listenfd), m_process_n(process_number), m_idx(-1), m_stop(false){
    assert((process_number > 0) && (process_number <= MAX_PROCESS_N));

    if((m_sub_process = new process[process_number]) == NULL) oops("fail new process[]");
    // 创建 process_number 个子进程，并建立它们和父进程之间的管道
    for(int i = 0; i < process_number; ++i){
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