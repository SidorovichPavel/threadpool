#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <concepts>

namespace threadpool
{
    template<class... Args>
    class datapool final
    {
    public:
        datapool(size_t threads_count, std::function<void(Args...)> task_func);
        ~datapool();

        void enqueue(Args&&... args);
        void wait() noexcept;
    private:
        std::function<void(Args...)> task_;
        std::mutex queue_mutex_;
        std::queue<std::tuple<Args...>> data_queue_;
        std::condition_variable condition_;
        bool stop_pool;

        std::vector<std::thread> workers_;
        std::atomic<size_t> work_counter_;
    };

    template<class ...Args>
    datapool<Args...>::datapool(size_t threads_count, std::function<void(Args...)> task_func)
        :
        stop_pool(false),
        task_(task_func)
    {
        for (size_t i = 0;i < threads_count;i++)
        {
            workers_.emplace_back([this]() {
                for (;;)
                {
                    std::unique_lock<std::mutex> locker(queue_mutex_);
                    condition_.wait(locker, [this] { return !data_queue_.empty() || stop_pool; });
                    if (stop_pool && data_queue_.empty())
                        return;

                    auto pack = std::move(data_queue_.top());
                    data_queue_.pop();
                    locker.unlock();

                    std::apply(task_, pack);
                    work_counter_--;
                }
                });
        }

    }

    template<class ...Args>
    datapool<Args...>::~datapool()
    {
        std::unique_lock<std::mutex> locker(queue_mutex_);
        stop_pool = true;
        locker.unlock();

        condition_.notify_all();
        for (auto& thread : workers_)
            thread.join();
    }

    template<class... Args>
    void datapool<Args...>::enqueue(Args&&... args)
    {
        std::unique_lock<std::mutex> locker(queue_mutex_);
        data_queue_.emplace(args...);
        locker.unlock();
        work_counter_++;
        condition_.notify_one();
    }

    template<class ...Args>
    void datapool<Args...>::wait() noexcept
    {
        for (;work_counter_ != 0u;);
    }

}