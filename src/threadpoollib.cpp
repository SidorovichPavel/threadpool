#include <algorithm>
#include <ranges>

#include <threadpoollib/threadpoollib.hpp>

namespace threadpool {

    threadpool::threadpool(size_t threads_count)
        :
        stop_pool_(false)
    {
        for (size_t i = 0; i < threads_count; ++i)
            workers_.emplace_back(std::thread([this]()
                {
                    for (;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> locker(queue_mutex_);
                            condition_.wait(locker, [this] { return !tasks_.empty() || stop_pool_; });
                            if (stop_pool_ && tasks_.empty())
                                return;

                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                        task();
                    }
                }));
    }

    threadpool::~threadpool()
    {
        std::unique_lock<std::mutex> locker(queue_mutex_);
        stop_pool_ = true;
        locker.unlock();

        condition_.notify_all();
        for (auto& thread : workers_)
            thread.join();
    }

} // namespace threadpool
