#pragma once

#include <libthreadpool/export.h>

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace threadpool {

class ThreadPool final {
public:
    ThreadPool(int _Count);
    ~ThreadPool();

    template<class Fn, class Obj, class... Args>
    auto enqueue(Fn&& _Fn, Obj&& _Obj, Args&&... _Args)
        -> std::future<decltype((_Obj->*_Fn)(std::forward<Args>(_Args)...))>;

    template<class Fn, class... Args>
    auto enqueue(Fn&& fn, Args&&... args) -> std::future<decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...))>;

private:
    std::mutex mQueueMutex;
    std::queue<std::function<void(void)>> mTasks;
    std::condition_variable mCondition;
    bool mStopPull;

    std::vector<std::thread> mWorkers;
};

template<class Fn, class Obj, class... Args>
auto ThreadPool::enqueue(Fn&& _Fn, Obj&& _Obj, Args&&... _Args)
    -> std::future<decltype((_Obj->*_Fn)(std::forward<Args>(_Args)...))>
{
    using ret_t = decltype((_Obj->*_Fn)(std::forward<Args>(_Args)...));

    auto pTask = new std::packaged_task<ret_t()>(
        std::bind(std::forward<Fn>(_Fn), std::forward<Obj>(_Obj), std::forward<Args>(_Args)...));

    std::future<ret_t> res = pTask->get_future();

    std::unique_lock<std::mutex> locker(mQueueMutex);
    if (mStopPull)
        throw std::runtime_error("ThreadPull, push task failed, How did you do it?");

    mTasks.emplace([pTask] {
        (*pTask)();
        delete pTask;
    });
    locker.unlock();
    mCondition.notify_one();
    return res;
}

template<class Fn, class... Args>
auto ThreadPool::enqueue(Fn&& fn, Args&&... args)
    -> std::future<decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...))>
{
    using ret_t = decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...));

    auto pTask = new std::packaged_task<ret_t()>(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));

    std::future<ret_t> res = pTask->get_future();

    std::unique_lock<std::mutex> locker(mQueueMutex);
    if (mStopPull)
        throw std::runtime_error("ThreadPull, push task failed, How did you do it?");

    mTasks.emplace([pTask] {
        (*pTask)();
        delete pTask;
    });

    locker.unlock();
    mCondition.notify_one();

    return res;
}

} // namespace threadpool
