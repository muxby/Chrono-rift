// ThreadPool.cpp - worker thread pool implementation

#include "ThreadPool.hpp"
#include <cstring>
#include <cerrno>

namespace ChronoRift {

ThreadPool::ThreadPool()
    : q_head(0), q_tail(0), q_count(0), running(false) {
    pthread_mutex_init(&q_mutex, nullptr);
    sem_init(&work_sem, 0, 0);   // starts at 0 — no tasks yet
    sem_init(&done_sem, 0, 0);   // starts at 0 — will be posted POOL_SIZE times
}

ThreadPool::~ThreadPool() {
    shutdown();
    pthread_mutex_destroy(&q_mutex);
    sem_destroy(&work_sem);
    sem_destroy(&done_sem);
}

bool ThreadPool::init() {
    running = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // detached so we dont have to join them later
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for (int i = 0; i < POOL_SIZE; ++i) {
        int rc = pthread_create(&threads[i], &attr, worker_loop, this);
        if (rc != 0) {
            running = false;
            pthread_attr_destroy(&attr);
            return false;
        }
    }
    pthread_attr_destroy(&attr);
    return true;
}

void ThreadPool::submit(TaskFn fn, void* arg) {
    if (!running || fn == nullptr) return;

    pthread_mutex_lock(&q_mutex);
    if (q_count < QUEUE_SIZE) {
        work_queue[q_tail] = fn;
        work_args[q_tail]  = arg;
        q_tail = (q_tail + 1) % QUEUE_SIZE;
        q_count++;
        sem_post(&work_sem);  // wake one worker
    }
    pthread_mutex_unlock(&q_mutex);
}

void ThreadPool::shutdown() {
    if (!running) return;
    running = false;

    // Wake all workers so they can exit their loops
    for (int i = 0; i < POOL_SIZE; ++i) {
        sem_post(&work_sem);
    }

    // Wait for all workers to drain (each posts done_sem once)
    for (int i = 0; i < POOL_SIZE; ++i) {
        sem_wait(&done_sem);
    }
}

// static worker entry point
void* ThreadPool::worker_loop(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while (pool->running) {
        // wait for something to appear in the queue
        int rc = sem_wait(&pool->work_sem);
        if (rc != 0 && errno == EINTR) continue;
        if (rc != 0) break;

        if (!pool->running) break;

        // Dequeue task
        pthread_mutex_lock(&pool->q_mutex);
        if (pool->q_count > 0) {
            TaskFn fn  = pool->work_queue[pool->q_head];
            void*  arg = pool->work_args[pool->q_head];
            pool->q_head = (pool->q_head + 1) % QUEUE_SIZE;
            pool->q_count--;
            pthread_mutex_unlock(&pool->q_mutex);

            // Execute task
            if (fn) fn(arg);
        } else {
            pthread_mutex_unlock(&pool->q_mutex);
        }
    }

    sem_post(&pool->done_sem);  // signal shutdown complete
    return nullptr;
}

} // namespace ChronoRift