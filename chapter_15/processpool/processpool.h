#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

// 子进程类
class process{
public:
    process(): m_pid(-1) {}

public:
    pid_t   m_pid;
    int     m_pipefd[2];
};

// 进程池类，模板参数是处理逻辑任务的类
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
    
private:
    static processpool<T>*      m_instance;     // 进程池实例
    process*                    m_sub_process;  // 进程池维护的资源 - 子进程数组
};

template<typename T>
processpool<T>* processpool<T>::m_instance = NULL;
