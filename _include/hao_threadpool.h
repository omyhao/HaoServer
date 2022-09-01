#ifndef _HAO_THREADPOOL_H_
#define _HAO_THREADPOOL_H_

#include <hao_timestamp.h>

#include <thread>
#include <vector>
#include <atomic>
#include <queue>
#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>
#include <future>

using std::vector;
using std::atomic;
using std::queue;
using std::thread;
using std::unique_ptr;
using std::mutex;
using std::function;
using std::condition_variable;
using std::future;
using std::unique_lock;
using std::shared_ptr;
using std::promise;
using std::make_shared;
using std::make_unique;
using std::scoped_lock;

class ThreadPool
{
    public:
        using concurrency_t = std::invoke_result_t<decltype(thread::hardware_concurrency)>;

        explicit ThreadPool();
        ~ThreadPool();
        void CreateThreads(const concurrency_t thread_count = 0);
        void WaitForTasks();
        void Reset(const concurrency_t thrad_count = 0);
        // 队列中任务数
        size_t WaitingCount() const;
        // 运行中任务数
        size_t RunningCount() const;
        // 总任务数 = 队列任务数+运行中任务数
        size_t TasksTotal() const;
        // 线程数
        concurrency_t ThreadCount() const;
        
        // 添加一个有返回值的任务
        // 参数可以有0个或多个
        template <typename F, typename... A, typename R>
        future<R> Submit(F&& task, A&&... args)
        {
            function<R()> task_function = std::bind(std::forward<F>(task), std::forward<A>(args)...);

            shared_ptr<promise<R>> task_promise = make_shared<promise<R>>();
            PushTask(
                [task_function, task_promise]
                {
                    try
                    {
                        if constexpr(std::is_void_v<R>)
                        {
                            std::invoke(task_function);
                            task_promise->set_value();
                        }
                        else
                        {
                            task_promise->set_value(std::invoke(task_function));
                        }
                    }
                    catch(...)
                    {
                        try
                        {
                            task_promise->set_exception(std::current_exception());
                        }
                        catch(...)
                        {

                        }
                    } 
                }
            );
            return task_promise->get_future();
        }

        // 添加一个不需要没有返回值的任务
        // 参数可以有0个或多个
        template <typename F, typename... A>
        void PushTask(F&& task, A&&... args)
        {
            function<void()> task_function = std::bind(std::forward<F>(task), std::forward<A>(args)...);
            {
                scoped_lock lock(tasks_mutex_);
                tasks_.push(task_function);
            }
            ++tasks_total_;
            task_available_cv_.notify_one();
        }
    private:
        
        void DestoryThreads();
        void WorkerThread();
        concurrency_t DetermineThreadCount(const concurrency_t thread_count);
    private:
        condition_variable task_available_cv_;
        condition_variable task_done_cv_;
        queue<function<void()>> tasks_;
        mutable mutex tasks_mutex_;
        concurrency_t thread_count_;
        atomic<size_t> tasks_total_;
        unique_ptr<thread[]> threads_;
        atomic<bool> waiting_;
        atomic<bool> running_;
        atomic<bool> paused_;

};

#endif