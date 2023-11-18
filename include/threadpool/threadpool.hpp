#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <ranges>
#include <algorithm>
#include <concepts>
#include <type_traits>

namespace threadpool
{

    class threadpool final
    {
    public:
        threadpool(size_t threads_count);
        ~threadpool();

        template <class Fn, class... Args>
        std::future<std::invoke_result_t<Fn, Args...>> enqueue(Fn&& fn, Args&&...args);

    private:

        std::mutex queue_mutex_;
        std::queue<std::move_only_function<void(void)>> tasks_;
        std::condition_variable condition_;
        bool stop_pool_;

        std::vector<std::thread> workers_;
    };

    template <typename Fn, class... Args>
    std::future<std::invoke_result_t<Fn, Args...>> threadpool::enqueue(Fn&& fn, Args&&...args)
    {
        using invoke_result_t = std::invoke_result_t<Fn, Args...>;

        auto pTask = std::make_unique<std::packaged_task<invoke_result_t()>>(
            [fn = std::forward<Fn>(fn), ...args = std::forward<Args>(args)]() mutable -> invoke_result_t {
                return std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
            });

        std::future<invoke_result_t> res = pTask->get_future();
        {
            std::lock_guard<std::mutex> locker(queue_mutex_);
            if (stop_pool_)
                throw std::runtime_error("ThreadPool, push task failed. How did you do it?");

            tasks_.emplace([utask = std::move(pTask)] {
                (*utask)();
                });

        }
        condition_.notify_one();

        return res;
    }

} // namespace threadpool
