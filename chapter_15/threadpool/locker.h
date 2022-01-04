#ifndef LOCKER_H
#define LOCKER_H


#include <exception>
#include <pthread.h>
#include <semaphore.h>
using std::exception;

/**
 * @brief 用类封装了信号量、互斥量、条件变量 
 * 注意：互斥锁  用于同步线程对共享数据的访问
 * 注意：条件变量用于同步线程间共享数据的值
 * 注意：互斥量和二元信号量的区别
 */


/* 封装信号量 */
class sem{
public:
    sem(){
        if(sem_init(&m_sem, 0, 0) != 0) throw exception();
    }
    ~sem(){
        sem_destroy(&m_sem);
    }

public:
    bool wait(){                                // -1
        return sem_wait(&m_sem) == 0;
    }
    bool post(){                                // +1
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t               m_sem;

};

/* 封装互斥锁 */
class locker{
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL) != 0) throw exception();
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }

public:
    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

private:
    pthread_mutex_t     m_mutex;
};

/* 封装条件变量 */
class cond{
public:
    cond(){
        if(pthread_mutex_init(&m_mutex, NULL) != 0) throw exception();
        if(pthread_cond_init(&m_cond, NULL) != 0) {
            pthread_mutex_destroy(&m_mutex);
            throw exception();
        }
    }
    ~cond(){
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }

public:
    bool wait(){
        int res = 0;
        pthread_mutex_lock(&m_mutex);
        res = pthread_cond_wait(&m_cond, &m_mutex);
        pthread_mutex_unlock(&m_mutex);
        return res == 0;
    }
    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }

private:
    pthread_mutex_t     m_mutex;
    pthread_cond_t      m_cond;
};
#endif