/*
 * Copyright (c) 2018 Ally of Intelligence Technology Co., Ltd. All rights reserved.
 *
 * Created by WuKun on 5/24/19.
 * Contact with:wk707060335@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http: *www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <queue>
#include <memory>
#include <iostream>
#include <type_traits>
#include <condition_variable>

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#include <boost/thread/future.hpp>

using boost::asio::post;
using boost::asio::dispatch;
using boost::asio::steady_timer;
std::mutex g_io_mutex;

class priority_handler_queue : public boost::asio::execution_context
{
public:
    // A class that satisfies the Executor requirements.
    class executor
    {
    public:
        executor(priority_handler_queue& q, int p) : context_(q), priority_(p) {}

        priority_handler_queue& context() const noexcept { return context_; }

        template <typename Function, typename Allocator>
        void dispatch(Function f, const Allocator& a) const
        {
            post(std::forward<Function>(f), a);
        }

        template <typename Function, typename Allocator>
        void post(Function f, const Allocator& a) const
        {
            auto p(std::allocate_shared<queued_handler<Function>>(
                    typename std::allocator_traits<
                            Allocator>::template rebind_alloc<char>(a),
                            priority_, std::move(f)));

            std::lock_guard<std::mutex> lock(context_.mutex_);
            context_.queue_.push(std::move(p));
            context_.condition_.notify_one();
        }

        template <typename Function, typename Allocator>
        void defer(Function f, const Allocator& a) const
        {
            post(std::forward<Function>(f), a);
        }

        void on_work_started() const noexcept {}
        void on_work_finished() const noexcept {}

        bool operator==(const executor& other) const noexcept
        {
            return &context_ == &other.context_ && priority_ == other.priority_;
        }

        bool operator!=(const executor& other) const noexcept
        {
            return !operator==(other);
        }

    private:
        priority_handler_queue& context_;
        int priority_;
    };

    executor get_executor(int pri = 0) noexcept
    {
        return executor(*const_cast<priority_handler_queue*>(this), pri);
    }

    void run()
    {
        while(!stopped_)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]{ return stopped_ || !queue_.empty(); });
            if (stopped_ || queue_.empty()) return;

            auto p(queue_.top());
            queue_.pop();
            lock.unlock();
            // make sure there is no mutex being locked
            // when user's code runs
            p->execute();
            lock.lock();
        }
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        condition_.notify_all();
    }

private:
    class queued_handler_base;
    using handler_ptr = typename std::shared_ptr<queued_handler_base>;

    class queued_handler_base
    {
    public:
        explicit queued_handler_base(int p) : priority_(p) {}
        virtual ~queued_handler_base() = default;

        virtual void execute() = 0;

        int priority_;
    };

    template <typename Function>
    class queued_handler : public queued_handler_base
    {
    public:
        queued_handler(int p, Function f) : queued_handler_base(p), function_(std::move(f)) {}

        void execute() override { function_(); }

    private:
        Function function_;
    };

    struct handler_comp
    {
        bool operator()(const handler_ptr& a, const handler_ptr& b)
        {
            return a->priority_ < b->priority_;
        }
    };

    std::mutex mutex_;
    std::condition_variable condition_;
    std::priority_queue<handler_ptr, std::vector<handler_ptr>, handler_comp> queue_;
    bool stopped_ = false;
};

int main()
{
    priority_handler_queue queue;

    auto low = queue.get_executor(0);
    auto med = queue.get_executor(1);
    auto high = queue.get_executor(2);

    for (int j = 0; j < 10; ++j) {
        dispatch(queue, [j] {

            sleep(1);
            std::lock_guard<std::mutex> lock(g_io_mutex);
            std::cout << std::this_thread::get_id() << " low:" << j << std::endl;
        });
    }

    for (int j = 0; j < 20; ++j) {
        dispatch(med, [j] {
            sleep(2);
            std::lock_guard<std::mutex> lock(g_io_mutex);
            std::cout << std::this_thread::get_id() << " med:" << j << std::endl;
        });
    }

    for (int j = 0; j < 5; ++j) {
        boost::packaged_task<int> task([=]() {
            sleep(1);
            return 111;
        });
        auto f = task.get_future();
        f.then([](boost::future<int> future) {
            std::lock_guard<std::mutex> lock(g_io_mutex);
            std::cout << std::this_thread::get_id() << " high:" << future.get() << std::endl;
        });

        post(high, std::move(task));
    }

    dispatch(queue.get_executor(-1), [&]{ queue.stop(); });

    boost::thread_group group_;
    for (std::size_t i = 0; i < std::thread::hardware_concurrency() - 1; ++i) {
        group_.create_thread([&](){ queue.run(); });
    }

    queue.run();

    group_.join_all();
}
