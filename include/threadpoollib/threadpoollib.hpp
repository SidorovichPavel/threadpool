#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <concepts>

#include "datapool.hpp"

namespace threadpool
{

    class threadpool final
    {
    public:
        threadpool(int _Count);
        ~threadpool();

        template <class Fn, class Obj, class... Args>
        auto enqueue(Fn&& fn, Obj&& obj, Args &&...args)
            -> std::future<decltype((obj->*fn)(std::forward<Args>(args)...))>;

        template <class Fn, class... Args>
        auto enqueue(Fn&& fn, Args &&...args) -> std::future<decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...))>;

    private:
        std::mutex queue_mutex_;
        std::queue<std::function<void(void)>> tasks_;
        std::condition_variable condition_;
        bool stop_pool;

        std::vector<std::thread> workers_;
    };

    template <class Fn, class Obj, class... Args>
    auto threadpool::enqueue(Fn&& fn, Obj&& obj, Args &&...args)
        -> std::future<decltype((obj->*fn)(std::forward<Args>(args)...))>
    {
        using ret_t = decltype((obj->*fn)(std::forward<Args>(args)...));

        auto pTask = new std::packaged_task<ret_t()>(
            std::bind(std::forward<Fn>(fn), std::forward<Obj>(obj), std::forward<Args>(args)...));

        std::future<ret_t> res = pTask->get_future();

        std::unique_lock<std::mutex> locker(queue_mutex_);
        if (stop_pool)
            throw std::runtime_error("ThreadPull, push task failed. How did you do it?");

        tasks_.emplace([pTask] {
            (*pTask)();
            delete pTask;
            });
        
        locker.unlock();
        condition_.notify_one();
        return res;
    }

    template <class Fn, class... Args>
    auto threadpool::enqueue(Fn&& fn, Args &&...args)
        -> std::future<decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...))>
    {
        using ret_t = decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...));

        auto pTask = new std::packaged_task<ret_t()>(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));

        std::future<ret_t> res = pTask->get_future();

        std::unique_lock<std::mutex> locker(queue_mutex_);
        if (stop_pool)
            throw std::runtime_error("ThreadPull, push task failed. How did you do it?");

        tasks_.emplace([pTask]
            {
                (*pTask)();
                delete pTask; });

        locker.unlock();
        condition_.notify_one();

        return res;
    }

} // namespace threadpool
