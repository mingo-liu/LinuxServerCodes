#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

template< typename T >
class threadpool
{
public:
    threadpool( int thread_number = 8, int max_requests = 10000 );
    ~threadpool();
    bool append( T* request );

private:
    static void* worker( void* arg );
    void run();

private:
    int m_thread_number;    // 线程池中的线程数
    int m_max_requests;     // 请求队列中的最大请求数
    pthread_t* m_threads;   // 描述线程池的数组
    std::list< T* > m_workqueue; // 请求队列, 双向链表
    locker m_queuelocker;   // 保护请求队列的互斥锁
    sem m_queuestat;        // 是否有任务需要处理
    bool m_stop;            // 是否结束线程
};

template< typename T >
threadpool< T >::threadpool( int thread_number, int max_requests ) : 
        m_thread_number( thread_number ), m_max_requests( max_requests ), m_stop( false ), m_threads( NULL )
{
    if( ( thread_number <= 0 ) || ( max_requests <= 0 ) )
    {
        throw std::exception();
    }

    m_threads = new pthread_t[ m_thread_number ];   // 创建线程池数组
    if( ! m_threads )
    {
        throw std::exception();
    }

    for ( int i = 0; i < thread_number; ++i )
    {
        printf( "create the %dth thread\n", i );
        if( pthread_create( m_threads + i, NULL, worker, this ) != 0 )   // 创建一个新的线程, worker是一个静态函数
        {
            delete [] m_threads;
            throw std::exception();
        }
        if( pthread_detach( m_threads[i] ) )  // 将线程设置为分离状态(脱离线程)，即该线程结束后自动释放其资源
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template< typename T >
threadpool< T >::~threadpool()
{
    delete [] m_threads;
    m_stop = true;
}

template< typename T >
bool threadpool< T >::append( T* request )
{
    m_queuelocker.lock();     // 操作请求队列时加锁, 因为请求队列被所有线程共享
    if ( m_workqueue.size() > m_max_requests )    // 为什么不是>=?
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back( request );
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template< typename T >
void* threadpool< T >::worker( void* arg )
{
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}

template< typename T >
void threadpool< T >::run()
{
    while ( ! m_stop )      // 每一个线程都在不停的从请求队列中取出任务并执行
    {
        m_queuestat.wait();  // 使用信号量是为了避免线程在任务队列为空时忙等待，通过等待信号量的通知来判断是否有任务需要处理。
        m_queuelocker.lock();
        if ( m_workqueue.empty() )    // 前面使用了信号量，为什么还要判断队列为空？wait同时唤醒多个进程，队列中的任务可能被其他线程取走，当前进程取任务时，队列为空
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();    // 删除链表头部元素
        m_queuelocker.unlock();
        if ( ! request )
        {
            continue;
        }
        request->process();       // 处理任务的具体逻辑
    }
}

#endif
