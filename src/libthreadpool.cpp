#include <libthreadpool/libthreadpool.hpp>

#include <algorithm>
#include <ranges>

namespace threadpool {

ThreadPool::ThreadPool(int _ThreadsCount): mStopPull(false)
{
    for (auto i = 0; i < _ThreadsCount; ++i)
        mWorkers.emplace_back(std::thread([this]() {
            for (;;) {
                std::function<void()> task;

                std::unique_lock<std::mutex> locker(mQueueMutex);
                mCondition.wait(locker, [this] { return !mTasks.empty() || mStopPull; });
                if (mStopPull && mTasks.empty())
                    return;

                task = std::move(mTasks.front());
                mTasks.pop();
                locker.unlock();

                task();
            }
        }));
}

ThreadPool::~ThreadPool()
{
    std::unique_lock<std::mutex> locker(mQueueMutex);
    mStopPull = true;
    locker.unlock();

    mCondition.notify_all();
    for (auto& thread: mWorkers)
        thread.join();
}

} // namespace threadpool
