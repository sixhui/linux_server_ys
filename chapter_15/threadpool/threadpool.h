#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <list>
#include <exception>
#include <pthread.h>

#include "locker.h"
using std::list;

/**
 * @brief 
 * 
 * 注意：使用工作队列 + 生产者消费者模式，解耦主线程和子线程
 * 注意：必须保证所有客户请求是无状态的，因为同一个连接上的不同请求可能会由不同的线程处理
 */
template<typename T>
class threadpool{
public:
    threadpool(int thread_n = 8, int max_requests = 10000);
    ~threadpool();

public:
    bool append(T* request);                    // 往请求队列添加任务

private:
    static void* worker(void* arg);             // 工作线程运行的函数 - 从请求队列取出任务并执行
    void run();


private:
    int             m_thread_n;                 // 线程池中的线程数
    int             m_max_requests;             // 请求队列中允许的最大请求数

    pthread_t*      m_threads;                  // 线程池数组
    list<T*>        m_workq;                    // 请求队列

    locker          m_qlocker;                  // 保护请求队列的互斥锁
    sem             m_qstat;                    // 是否有任务需要处理
    bool            m_stop;                     // 是否结束线程

};



#endif

