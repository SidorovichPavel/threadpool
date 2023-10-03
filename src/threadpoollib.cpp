#include <threadpoollib/threadpoollib.hpp>

#include <algorithm>
#include <ranges>

namespace threadpool {

    threadpool::threadpool(int threads_count)
        : stop_pool(false)
    {
        for (auto i = 0; i < threads_count; ++i)
            workers_.emplace_back(std::thread([this]() {
            for (;;) {
                std::function<void()> task;

                std::unique_lock<std::mutex> locker(queue_mutex_);
                condition_.wait(locker, [this] { return !tasks_.empty() || stop_pool; });
                if (stop_pool && tasks_.empty())
                    return;

                task = std::move(tasks_.front());
                tasks_.pop();
                locker.unlock();

                task();
            }
                }));
    }

    threadpool::~threadpool()
    {
        std::unique_lock<std::mutex> locker(queue_mutex_);
        stop_pool = true;
        locker.unlock();

        condition_.notify_all();
        for (auto& thread : workers_)
            thread.join();
    }

} // namespace threadpool
