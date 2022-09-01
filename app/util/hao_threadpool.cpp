#include "hao_threadpool.h"

ThreadPool::ThreadPool()
    : waiting_{false}, running_{false},paused_{false},
      tasks_total_{0},
      thread_count_{0},
      threads_{nullptr}
{
}

ThreadPool::~ThreadPool()
{
    WaitForTasks();
    DestoryThreads();
}

size_t ThreadPool::WaitingCount() const
{
    scoped_lock task_lock{tasks_mutex_};
    return tasks_.size();
}

size_t ThreadPool::RunningCount() const
{
    scoped_lock task_lock{tasks_mutex_};
    return tasks_total_ - tasks_.size();
}

size_t ThreadPool::TasksTotal() const
{
    return tasks_total_;
}

ThreadPool::concurrency_t ThreadPool::ThreadCount() const
{
    return thread_count_;
}

void ThreadPool::WaitForTasks()
{
    waiting_ = true;
    unique_lock<mutex> tasks_lock{tasks_mutex_};
    task_done_cv_.wait(tasks_lock, [&]{
        return (tasks_total_ == (paused_? tasks_.size() : 0));
    });
    waiting_ = false;
}

ThreadPool::concurrency_t ThreadPool::DetermineThreadCount(const concurrency_t thread_count)
{
    if(thread_count > 0)
    {
        return thread_count;
    }
    else
    {
        if(thread::hardware_concurrency() > 0)
        {
            return thread::hardware_concurrency();
        }
        else
        {
            return 1;
        }
    }
}

void ThreadPool::Reset(const concurrency_t thread_count)
{
    const bool  was_paused = paused_;
    paused_ =  true;
    WaitForTasks();
    DestoryThreads();
    thread_count_ = DetermineThreadCount(thread_count);
    threads_ = make_unique<thread[]>(thread_count);
    paused_ = was_paused;
    CreateThreads();
}

void ThreadPool::CreateThreads(const concurrency_t  thread_count)
{
    thread_count_ = DetermineThreadCount(thread_count);
    threads_ = make_unique<thread[]>(DetermineThreadCount(thread_count));
    running_ = true;
    for(concurrency_t i{0}; i < thread_count_; ++i)
    {
        threads_[i] = thread(&ThreadPool::WorkerThread, this);
    }
}

void ThreadPool::DestoryThreads()
{
    running_ = false;
    task_available_cv_.notify_all();
    for(concurrency_t i{0}; i < thread_count_; ++i)
    {
        threads_[i].join();
    }
}

void ThreadPool::WorkerThread()
{
    while(running_)
    {
        function<void()> task;
        unique_lock<mutex> tasks_lock{tasks_mutex_};
        task_available_cv_.wait(tasks_lock, [&]{
            return !tasks_.empty() || !running_;
        });
        if(running_ && !paused_)
        {
            task = move(tasks_.front());
            tasks_.pop();
            tasks_lock.unlock();
            task();
            tasks_lock.lock();
            --tasks_total_;
            if(waiting_)
            {
                task_done_cv_.notify_one();
            }
        }
    }
}