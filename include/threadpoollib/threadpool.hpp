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
#include <ranges>

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
    concept CallableWithValue = requires (Callable fn, Container container)
    {
        { fn(*container.begin()) };
    };

    template<class Callable, class Container, class ResIter>
    concept RangeToIterUnaryTransformer = requires (Container container, ResIter result, Callable fn)
    {
        { *result = fn(*container.begin()) };
    };

    template<class Callable, class InIter, class ResIter>
    concept IterToIterUnaryTransformer = requires (InIter iter, ResIter result, Callable fn)
    {
        { *result = fn(*iter) };
    };

    template<class Callable, class Conteiner1, class Conteiner2>
    concept RangeToRangeUnaryTransformer = requires(Conteiner1 conteiner1, Conteiner2 conteiner2, Callable fn)
    {
        { *conteiner2.begin() = fn(*conteiner1.begin()) };
    };

    template<class Callable, class Conteiner1, class Conteiner2, class ResIter>
    concept RangesToIterBinaryTransformer = requires (Conteiner1 conteiner1, Conteiner2 conteiner2, ResIter result, Callable fn)
    {
        { *result = fn(*conteiner1.begin(), *conteiner2.begin()) };
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

        //unary transform for range input
        template<std::ranges::range Range, RandomAccessIterator ResIter, RangeToIterUnaryTransformer<Range, ResIter> Callable>
        void transform(Range&& range, ResIter&& result, Callable&& fn);

        //unary transform for iterators
        template<class InIter, class ResIter, IterToIterUnaryTransformer<InIter, ResIter> Callable>
        void transform(InIter&& first, InIter&& last, ResIter&& result, Callable&& fn);

        //unary transform for ranges
        template<std::ranges::range InRange, std::ranges::range OutRange, RangeToRangeUnaryTransformer<InRange, OutRange> Callable>
        void transform(InRange&& in_range, OutRange&& out_range, Callable&& fn);

        //binary transform for ranges input
        template<std::ranges::range R1, std::ranges::range R2, RandomAccessIterator ResIter,
            RangesToIterBinaryTransformer<R1, R2, ResIter> Callable>
        void transform(R1&& range1, R2&& range2, ResIter&& result, Callable&& fn);

    private:

        std::mutex queue_mutex_;
        std::queue<std::function<void(void)>> tasks_;
        std::condition_variable condition_;
        bool stop_pool_;

        std::vector<std::thread> workers_;
    };

    template <class Fn, class... Args>
    std::future<std::invoke_result_t<Fn, Args...>> threadpool::enqueue(Fn&& fn, Args&&...args)
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

    template<class InIter, class ResIter, IterToIterUnaryTransformer<InIter, ResIter> Callable>
    void threadpool::transform(InIter&& first, InIter&& last, ResIter&& result, Callable&& fn)
    {
        transform(std::ranges::subrange(first, last), result, fn);
    }

    template<std::ranges::range Range, RandomAccessIterator ResIter, RangeToIterUnaryTransformer<Range, ResIter> Callable>
    void threadpool::transform(Range&& range, ResIter&& result, Callable&& fn)
    {
        size_t chunk_count = std::thread::hardware_concurrency();
        size_t chunk_size = range.size() / chunk_count;

        auto chunks = range | std::views::chunk(chunk_size);

        std::atomic<size_t> counter = chunks.size();
        std::mutex task_mutex;
        std::condition_variable done;
        auto subtransform = [&](auto&& range, auto iter)
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

            for (auto&& chunk : chunks)
            {
                tasks_.emplace(std::bind(subtransform, chunk, result));
                result += chunk_size;
            }
        }
        condition_.notify_all();

        std::unique_lock<std::mutex> lock(task_mutex);
        done.wait(lock, [&] {return counter == 0;});
    }

    template<std::ranges::range InRange, std::ranges::range OutRange, RangeToRangeUnaryTransformer<InRange, OutRange> Callable>
    void threadpool::transform(InRange&& in_range, OutRange&& out_range, Callable&& fn)
    {
        size_t chunk_count = std::thread::hardware_concurrency();
        size_t chunk_size = in_range.size() / chunk_count;

        auto chunks_ir_zip = std::views::zip(
            in_range | std::views::chunk(chunk_size),
            out_range | std::views::chunk(chunk_size)
        );

        std::atomic<size_t> counter = chunks_ir_zip.size();
        std::mutex task_mutex;
        std::condition_variable done;
        auto transform = [&](auto&& range1, auto&& range2)
            {
                std::ranges::transform(range1, range2.begin(), fn);
                std::unique_lock<std::mutex> lock(task_mutex);
                counter--;
                done.notify_one();
            };

        {
            std::unique_lock<std::mutex> locker(queue_mutex_);
            if (stop_pool_)
                throw std::runtime_error("ThreadPull, push task failed. How did you do it?");


            for (auto&& [in, res] : chunks_ir_zip)
            {
                tasks_.emplace(std::bind(transform, in, res));
            }
        }
        condition_.notify_all();

        std::unique_lock<std::mutex> lock(task_mutex);
        done.wait(lock, [&] {return counter == 0;});
    }

    template<std::ranges::range R1, std::ranges::range R2, RandomAccessIterator ResIter,
        RangesToIterBinaryTransformer<R1, R2, ResIter> Callable>
    void threadpool::transform(R1&& range1, R2&& range2, ResIter&& result, Callable&& fn)
    {
        size_t chunk_count = std::thread::hardware_concurrency();
        size_t chunk_size = range1.size() / chunk_count;

        auto chunks_ii_zip = std::views::zip(
            range1 | std::views::chunk(chunk_size),
            range2 | std::views::chunk(chunk_size)
        );

        std::atomic<size_t> counter = chunks_ii_zip.size();
        std::mutex task_mutex;
        std::condition_variable done;
        auto transform = [&](auto&& range1, auto&& range2, auto res_iter)
            {
                std::ranges::transform(range1, range2, res_iter, fn);
                std::unique_lock<std::mutex> lock(task_mutex);
                counter--;
                done.notify_one();
            };

        {
            std::unique_lock<std::mutex> locker(queue_mutex_);
            if (stop_pool_)
                throw std::runtime_error("ThreadPull, push task failed. How did you do it?");

            for (auto&& [ch1, ch2] : chunks_ii_zip)
            {
                tasks_.emplace(std::bind(transform, ch1, ch2, result));
                result += chunk_size;
            }
        }
        condition_.notify_all();

        std::unique_lock<std::mutex> lock(task_mutex);
        done.wait(lock, [&] {return counter == 0;});
    }

} // namespace threadpool
