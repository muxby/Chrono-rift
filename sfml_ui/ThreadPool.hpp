// ThreadPool.hpp - fixed size pool of detached worker threads with a circular queue

#pragma once

#include <pthread.h>
#include <semaphore.h>

namespace ChronoRift {

static constexpr int POOL_SIZE = 4;
static constexpr int QUEUE_SIZE = 32;

using TaskFn = void* (*)(void*);

class ThreadPool {
public:
    ThreadPool();
    ~ThreadPool();

    bool init();               // spins up POOL_SIZE detached threads, call before submit
    void submit(TaskFn fn, void* arg);  // enqueue a task, thread-safe
    void shutdown();           // stops all workers and waits for them to finish

private:
    static void* worker_loop(void* arg);

    TaskFn       work_queue[QUEUE_SIZE];
    void*        work_args[QUEUE_SIZE];
    int          q_head;   // next to dequeue
    int          q_tail;   // next to enqueue
    int          q_count;  // items in queue

    pthread_t    threads[POOL_SIZE];
    bool         running;

    sem_t        work_sem;      // counts available tasks in queue
    sem_t        done_sem;      // each worker posts this on exit
    pthread_mutex_t q_mutex;    // protects the queue
};

} // namespace ChronoRift