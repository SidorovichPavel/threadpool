#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <concepts>
#include <type_traits>

namespace threadpool
{
    template<class T>
    concept Pointer = std::is_pointer<T>::value;

    template <typename Iter>
    concept RandomAccessIterator = requires(Iter it) {
        { it + 1 } -> std::same_as<Iter>;
        { it - 1 } -> std::same_as<Iter>;
        { it += 1 } -> std::same_as<Iter&>;
        { it -= 1 } -> std::same_as<Iter&>;
        { it - it } -> std::convertible_to<std::ptrdiff_t>;
        { it + std::ptrdiff_t(1) } -> std::same_as<Iter>;
        { std::distance(it, it) } -> std::same_as<std::ptrdiff_t>;
    };

    template<class Container, class Callable>
    concept CallableWithValue = requires (Callable callable, Container container)
    {
        { callable(*container.begin()) };
    };

    template<class Container, class ResIter, class Callable>
    concept UnaryTransform = requires (Container container, ResIter result, Callable callable)
    {
        { *result = callable(*container.begin()) };
    };

    template<class Conteiner, class Iter, class ResIter, class Callable>
    concept BinaryTransform = requires (Conteiner conteiner, Iter iter, ResIter result, Callable callable)
    {
        { *result = callable(*conteiner.begin(), *iter) };
    };

    class threadpool final
    {
    public:
        threadpool(size_t threads_count);
        ~threadpool();

        template <class Fn, class... Args>
        std::future<std::invoke_result_t<Fn, Args...>> enqueue(Fn&& fn, Args &&...args);

        template<std::ranges::range Range, class Callable>
        void for_each(Range&& range, Callable&& fn);

        template<std::ranges::range Range, RandomAccessIterator ResIter, class Callable>
        void transform(Range&& range, ResIter result, Callable&& fn);

        template<std::ranges::range Range, RandomAccessIterator Iter, RandomAccessIterator ResIter, class Callback>
        void transform(Range&& range, Iter&& iter, ResIter&& result, Callback&& fn);
    private:

        std::mutex queue_mutex_;
        std::queue<std::function<void(void)>> tasks_;
        std::condition_variable condition_;
        bool stop_pool_;

        std::vector<std::thread> workers_;
    };

    template <class Fn, class... Args>
    std::future<std::invoke_result_t<Fn, Args...>> threadpool::enqueue(Fn&& fn, Args &&...args)
    {
        using invoke_result_t = std::invoke_result_t<Fn, Args...>;

        auto pTask = new std::packaged_task<invoke_result_t()>(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));

        std::future<invoke_result_t> res = pTask->get_future();

        std::unique_lock<std::mutex> locker(queue_mutex_);
        if (stop_pool_)
            throw std::runtime_error("ThreadPull, push task failed. How did you do it?");

        tasks_.emplace([pTask]
            {
                (*pTask)();
                delete pTask;
            });

        locker.unlock();
        condition_.notify_one();

        return res;
    }

    template<std::ranges::range Range, class Callable>
    void threadpool::for_each(Range&& range, Callable&& fn)
    {
        static_assert(CallableWithValue<Range, Callable>, "Callable must be invocable with Conteiner::value_type.");

        size_t chunk_count = std::thread::hardware_concurrency();
        size_t chunk_size = range.size() / chunk_count;

        auto chunks = range | std::views::chunk(chunk_size);
        std::atomic<size_t> counter = chunks.size();
        std::mutex task_mutex;
        std::condition_variable done;

        {
            std::unique_lock<std::mutex> locker(queue_mutex_);
            if (stop_pool_)
                throw std::runtime_error("ThreadPull, push task failed. How did you do it?");

            for (auto&& chunk : chunks)
            {
                tasks_.emplace([&]
                    {
                        std::ranges::for_each(chunk, fn);

                        std::unique_lock<std::mutex> lock(task_mutex);
                        counter--;
                        done.notify_one();
                    });
            }
        }
        condition_.notify_all();

        std::unique_lock<std::mutex> lock(task_mutex);
        done.wait(lock, [&] {return counter == 0;});
    }

    template<std::ranges::range Range, RandomAccessIterator ResIter, class Callable>
    void threadpool::transform(Range&& range, ResIter result, Callable&& fn)
    {
        static_assert(UnaryTransform<Range, ResIter, Callable>, "Callable must be compile. *result = fn(*range.begin()) must be correct");

        size_t chunk_count = std::thread::hardware_concurrency();
        size_t chunk_size = range.size() / chunk_count;

        auto chunks = range | std::views::chunk(chunk_size);

        std::atomic<size_t> counter = chunks.size();
        std::mutex task_mutex;
        std::condition_variable done;
        auto transform = [&](auto&& range, auto iter)
            {
                std::ranges::transform(range, iter, fn);
                std::unique_lock<std::mutex> lock(task_mutex);
                counter--;
                done.notify_one();
            };

        {
            std::unique_lock<std::mutex> locker(queue_mutex_);
            if (stop_pool_)
                throw std::runtime_error("ThreadPull, push task failed. How did you do it?");

            auto res = result;
            for (auto&& chunk : chunks)
            {
                tasks_.emplace(std::bind(transform, chunk, res));
                res += chunk_size;
            }
        }
        condition_.notify_all();

        std::unique_lock<std::mutex> lock(task_mutex);
        done.wait(lock, [&] {return counter == 0;});
    }

    template<std::ranges::range Range, RandomAccessIterator Iter, RandomAccessIterator ResIter, class Callable>
    void threadpool::transform(Range&& range, Iter&& iter, ResIter&& result, Callable&& fn)
    {
        static_assert(UnaryTransform<Range, Iter, ResIter, Callable>, "Callable must be compile. *result = fn(*range.begin(), *iter) must be correct");

        size_t chunk_count = std::thread::hardware_concurrency();
        size_t chunk_size = range.size() / chunk_count;

        auto chunks = range | std::views::chunk(chunk_size);

        std::atomic<size_t> counter = chunks.size();
        std::mutex task_mutex;
        std::condition_variable done;
        auto transform = [&](auto&& range, auto iter, auto res_iter)
            {
                std::ranges::transform(range, iter, res_iter, fn);
                std::unique_lock<std::mutex> lock(task_mutex);
                counter--;
                done.notify_one();
            };

        {
            std::unique_lock<std::mutex> locker(queue_mutex_);
            if (stop_pool_)
                throw std::runtime_error("ThreadPull, push task failed. How did you do it?");

            auto res = result;
            auto it = iter;
            for (auto&& chunk : chunks)
            {
                tasks_.emplace(std::bind(transform, chunk, it, res));
                res += chunk_size;
                it += chunk_size;
            }
        }
        condition_.notify_all();

        std::unique_lock<std::mutex> lock(task_mutex);
        done.wait(lock, [&] {return counter == 0;});
    }

} // namespace threadpool
