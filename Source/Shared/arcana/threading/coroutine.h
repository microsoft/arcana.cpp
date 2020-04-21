//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifdef __cpp_coroutines

#include <cassert>

#include <arcana/threading/task.h>
#include <experimental/coroutine>
#include <optional>

namespace arcana
{
    namespace
    {
        expected<void, std::error_code> coroutine_success = expected<void, std::error_code>::make_valid();
    }

    namespace internal
    {
        class unobserved_error : public std::system_error
        {
        public:
            unobserved_error(std::error_code error) : std::system_error(error) {}
        };

        inline void UnhandledException()
        {
            assert(false && "Unhandled exception. Arcana task returning functions should handle exceptions as arcana tasks do not support them.");
            std::terminate();
        }

        template <typename ResultT>
        inline void HandleCoroutineException(std::exception_ptr e, arcana::task_completion_source<ResultT, std::error_code>& taskCompletionSource)
        {
            try
            {
                std::rethrow_exception(std::move(e));
            }
            catch (const unobserved_error& error)
            {
                taskCompletionSource.complete(arcana::make_unexpected(error.code()));
            }
            catch (...)
            {
                UnhandledException();
            }
        }

        template <typename ResultT>
        inline void HandleCoroutineException(std::exception_ptr e, arcana::task_completion_source<ResultT, std::exception_ptr>& taskCompletionSource)
        {
            taskCompletionSource.complete(arcana::make_unexpected(e));
        }

        template <typename ResultT, typename ErrorT>
        class base_promise_type
        {
        public:
            auto get_return_object() { return m_taskCompletionSource.as_task(); }
            std::experimental::suspend_never initial_suspend() { return {}; }
            std::experimental::suspend_never final_suspend() { return {}; }

            // TODO: Required by Clang 5-7 as it's built against a different version of the Coroutines TS (see https://clang.llvm.org/cxx_status.html). Remove once Clang updates to newer Coroutines TS.
#ifdef __clang__
            void unhandled_exception() { UnhandledException(); }
#else
            void set_exception(std::exception_ptr e) { HandleCoroutineException(std::move(e), m_taskCompletionSource); }
#endif

        protected:
            base_promise_type() = default;
            base_promise_type(const base_promise_type&) = default;
            ~base_promise_type() = default;
            arcana::task_completion_source<ResultT, ErrorT> m_taskCompletionSource;
        };

        template <typename ResultT, typename ErrorT>
        class value_promise_type : public base_promise_type<ResultT, ErrorT>
        {
        public:
            template <typename T>
            void return_value(T&& result) { m_taskCompletionSource.complete(std::forward<T>(result)); }
        };

        template <typename ErrorT>
        class void_promise_type : public base_promise_type<void, ErrorT>
        {
        public:
            void return_void() { m_taskCompletionSource.complete(); }
        };
    }

    // This enables generating a task<ResultT, ErrorT> return value from a coroutine. For example:
    // task<int, std::error_code> DoSomethingAsync()
    // {
    //     co_return 42;
    // }
    template <typename ResultT, typename ErrorT, typename... Args>
    struct std::experimental::coroutine_traits<task<ResultT, ErrorT>, Args...>
    {
        using promise_type = arcana::internal::value_promise_type<ResultT, ErrorT>;
    };

    // This enables generating a task<void, std::exception_ptr> return value from a coroutine. For example:
    // task<void, std::exception_ptr> DoSomethingAsync()
    // {
    //     co_return;
    // }
    // NOTE: This is enabled for the std::exception_ptr case only, not std::error_code. This is because
    // the promise_type can't have both a return_void and a return_value, which means it would not be
    // able to support doing a co_return of an std::error_code. Because of this, we instead have to
    // always co_return a value in the std::error_code case, and in the case of a task<void, std::error_code>
    // coroutine that is successful, we have to return coroutine_success.
    template <typename... Args>
    struct std::experimental::coroutine_traits<task<void, std::exception_ptr>, Args...>
    {
        using promise_type = arcana::internal::void_promise_type<std::exception_ptr>;
    };

    // Microsoft's C++ standard library currently defines its own coroutine_traits for std::future.
    // For other compilers we need to define our own, as defining this isn't currently part of the ISO C++ standard.
    // _RESUMABLE_FUNCTIONS_SUPPORTED is the current macro used by the Microsoft C++ standard library to trigger definition of these traits.
#ifndef _RESUMABLE_FUNCTIONS_SUPPORTED
    template <typename... Args>
    struct std::experimental::coroutine_traits<std::future<void>, Args...>
    {
        class promise_type
        {
        public:
            auto get_return_object() { return m_promise.get_future(); }
            std::experimental::suspend_never initial_suspend() { return {}; }
            std::experimental::suspend_never final_suspend() { return {}; }
            // TODO: Replace this with set_exception once Clang updates to newer Coroutines TS.
            void unhandled_exception() { arcana::internal::UnhandledException(); }
            void return_void() { m_promise.set_value(); }

        private:
            std::promise<void> m_promise;
        };
    };

    template <typename ResultT, typename... Args>
    struct std::experimental::coroutine_traits<std::future<ResultT>, Args...>
    {
        class promise_type
        {
        public:
            auto get_return_object() { return m_promise.get_future(); }
            std::experimental::suspend_never initial_suspend() { return {}; }
            std::experimental::suspend_never final_suspend() { return {}; }
            // TODO: Replace this with set_exception once Clang updates to newer Coroutines TS.
            void unhandled_exception() { arcana::internal::UnhandledException(); }

            template <typename T>
            void return_value(T&& result) { m_promise.set_value(std::forward<T>(result)); }

        private:
            std::promise<ResultT> m_promise;
        };
    };
#endif

    namespace internal
    {
        // The task_awaiter_result class (plus void specialization) wrap an expected<ResultT, std::error_code> and ensure that any errors are observed.
        // If the expected<ResultT, std::error_code> is in an error state and an attempt is made to access the value, an unobserved_error exception is thrown.
        //   This supports scenairos where a value is directly obtained:
        //   int result = co_await SomeTaskOfIntReturningFunction();
        // If the expected<ResultT, std::error_code> is in an error state and is destroyed when the error has not been observed, an unobserved_error exception is thrown.
        //   This supports scenarios where the result of the co_await is ignored:
        //   co_await SomeTaskReturningFunctionThatResultsInAnError(); 
        // This is propagated up to the coroutine_traits, and in the case of the arcana task coroutine_traits, the final task is set to an error
        // state and contains the unobserved error (thereby propagating the error all the way to the caller).
        template <typename ResultT>
        class base_task_awaiter_result
        {
        public:
            operator arcana::expected<ResultT, std::error_code>()
            {
                m_observed = true;
                return m_expected;
            }

        protected:
            base_task_awaiter_result(expected<ResultT, std::error_code>&& expected) :
                m_expected(std::move(expected))
            {
            }

            base_task_awaiter_result(base_task_awaiter_result&& other) :
                m_expected(std::move(other.m_expected)),
                m_observed(other.m_observed)
            {
                other.m_observed = true;
            }

            base_task_awaiter_result(const base_task_awaiter_result&) = delete;
            base_task_awaiter_result& operator=(const base_task_awaiter_result&) = delete;

            ~base_task_awaiter_result() noexcept(false)
            {
                if (!m_observed && m_expected.has_error())
                {
                    throw unobserved_error(m_expected.error());
                }
            }

            expected<ResultT, std::error_code> m_expected;
            bool m_observed{ false };
        };

        template <typename ResultT>
        class task_awaiter_result : public base_task_awaiter_result<ResultT>
        {
        public:
            template<typename T>
            task_awaiter_result(T&& t)
                : base_task_awaiter_result<ResultT>{ std::forward<T>(t) }
            {}

            operator ResultT()
            {
                m_observed = true;
                if (m_expected.has_error())
                {
                    throw unobserved_error(m_expected.error());
                }

                return m_expected.value();
            }
        };

        template<>
        class task_awaiter_result<void> : public base_task_awaiter_result<void>
        {
        public:
            template<typename T>
            task_awaiter_result(T&& t)
                : base_task_awaiter_result<void>{ std::forward<T>(t) }
            {}
        };

        template <typename SchedulerT, typename ResultT, typename ErrorT>
        class base_task_awaiter
        {
        public:
            base_task_awaiter(SchedulerT& scheduler, arcana::task<ResultT, ErrorT> task) : m_scheduler(scheduler), m_task(std::move(task)) {}
            bool await_ready() { return false; }
            void await_suspend(std::experimental::coroutine_handle<> coroutine)
            {
                auto continuation = m_task.then(m_scheduler, arcana::cancellation::none(),
                    [this, coroutine = std::move(coroutine)](const arcana::expected<ResultT, ErrorT>& result) noexcept(std::is_same<ErrorT, std::error_code>::value)
                {
                    m_result = result;
                    coroutine.resume();
                });

                static_assert(std::is_same<decltype(continuation), arcana::task<void, ErrorT>>::value, "ErrorT of continuation should match ErrorT of task, otherwise we can introduce a try/catch (through task internals) in code that does not have exceptions enabled.");
            }
        protected:
            using base = base_task_awaiter;
            base_task_awaiter() = delete;
            base_task_awaiter(const base_task_awaiter&) = default;
            ~base_task_awaiter() = default;
            std::optional<arcana::expected<ResultT, ErrorT>> m_result;
        private:
            SchedulerT & m_scheduler;
            arcana::task<ResultT, ErrorT> m_task;
        };
    }

    // This enables awaiting a task<ResultT, std::error_code> within a coroutine. For example:
    // std::future<int> DoAnotherThingAsync()
    // {
    //     int result = co_await configure_await(arcana::inline_scheduler, DoSomethingAsync());
    //     return result;
    // }
    // - or -
    // std::future<int> DoAnotherThingAsync()
    // {
    //     expected<int, std::error_code> result = co_await configure_await(arcana::inline_scheduler, DoSomethingAsync());
    //     return result.value();
    // }
    template <typename SchedulerT, typename ResultT>
    inline auto configure_await(SchedulerT& scheduler, task<ResultT, std::error_code> task)
    {
        class task_awaiter : private arcana::internal::base_task_awaiter<SchedulerT, ResultT, std::error_code>
        {
        public:
            using base::base;
            using base::await_ready;
            using base::await_suspend;
            auto await_resume()
            {
                return arcana::internal::task_awaiter_result<ResultT>(std::move(*m_result)); 
            }
        };

        return task_awaiter{ scheduler, std::move(task) };
    }

    // This enables awaiting a task<ResultT, std::exception_ptr> within a coroutine. For example:
    // std::future<int> DoAnotherThingAsync()
    // {
    //     auto result = co_await configure_await(arcana::inline_scheduler, DoSomethingAsync());
    //     return result;
    // }
    template <typename SchedulerT, typename ResultT>
    inline auto configure_await(SchedulerT& scheduler, task<ResultT, std::exception_ptr> task)
    {
        class task_awaiter : private arcana::internal::base_task_awaiter<SchedulerT, ResultT, std::exception_ptr>
        {
        public:
            using base::base;
            using base::await_ready;
            using base::await_suspend;
            ResultT await_resume()
            {
                if (m_result->has_error())
                {
                    std::rethrow_exception(m_result->error());
                }

                return std::move(m_result->value());
            }
        };

        return task_awaiter{ scheduler, std::move(task) };
    }

    // This enables awaiting a task<void, std::exception_ptr> within a coroutine. For example:
    // std::future<void> DoAnotherThingAsync()
    // {
    //     co_await configure_await(arcana::inline_scheduler, DoSomethingAsync());
    // }
    template <typename SchedulerT>
    inline auto configure_await(SchedulerT& scheduler, task<void, std::exception_ptr> task)
    {
        class task_awaiter : private arcana::internal::base_task_awaiter<SchedulerT, void, std::exception_ptr>
        {
        public:
            using base::base;
            using base::await_ready;
            using base::await_suspend;
            void await_resume()
            {
                if (m_result->has_error())
                {
                    std::rethrow_exception(m_result->error());
                }
            }
        };

        return task_awaiter{ scheduler, std::move(task) };
    }

    // This enables awaiting a scheduler (e.g. switching scheduler/dispatcher contexts). For example:
    // std::future<void> DoSomethingAsync()
    // {
    //    // do some stuff in the current scheduling context
    //    co_await switch_to(background_dispatcher);
    //    // do some stuff on a background thread
    //    co_await switch_to(render_dispatcher);
    //    // do some stuff on the render thread
    // }
    template <typename SchedulerT>
    inline auto switch_to(SchedulerT& scheduler)
    {
        class scheduler_awaiter
        {
        public:
            scheduler_awaiter(SchedulerT& scheduler) : m_scheduler(scheduler) {}
            bool await_ready() { return false; }
            void await_resume() {}
            void await_suspend(std::experimental::coroutine_handle<> coroutine)
            {
                m_scheduler([coroutine = std::move(coroutine)]
                {
                    coroutine.resume();
                });
            }
        private:
            SchedulerT& m_scheduler;
        };

        return scheduler_awaiter{ scheduler };
    }
}

#endif
