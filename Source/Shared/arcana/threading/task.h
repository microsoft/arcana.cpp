#pragma once

#include "arcana/expected.h"
#include "arcana/functional/inplace_function.h"
#include "arcana/iterators.h"
#include "arcana/type_traits.h"

#include "cancellation.h"

#include <gsl/gsl>
#include <memory>
#include <stdexcept>

#include <atomic>

namespace arcana
{
    template<typename ResultT, typename ErrorT>
    class task_completion_source;

    //
    // A scheduler that will invoke the continuation inline
    // right after the previous task.
    //
    namespace
    {
        constexpr auto inline_scheduler = [](auto&& callable)
        {
            callable();
        };
    }
}

#include "internal/internal_task.h"

namespace arcana
{
    //
    //  Generic task system to run work with continuations on a generic scheduler.
    //
    //  The scheduler on which tasks are queued must satisfy this contract:
    //
    //      struct scheduler
    //      {
    //          template<CallableT>
    //          void queue(CallableT&& callable)
    //          {
    //              callable must be usable like this: callable();
    //          }
    //      };
    //
    template<typename ResultT, typename ErrorT>
    class task
    {
        using payload_t = internal::task_payload_with_return<ResultT, ErrorT>;
        using payload_ptr = std::shared_ptr<payload_t>;

        static_assert(std::is_same<typename as_expected<ResultT, ErrorT>::value_type, ResultT>::value,
            "task can't be of expected<T>");

    public:
        using result_type = ResultT;
        using error_type = ErrorT;

        task() = default;

        task(const task& other) = default;
        task(task&& other) = default;

        task(const task_completion_source<ResultT, ErrorT>& source)
            : m_payload{ source.m_payload }
        {}

        task(task_completion_source<ResultT, ErrorT>&& source)
            : m_payload{ std::move(source.m_payload) }
        {}

        task& operator=(task&& other) = default;
        task& operator=(const task& other) = default;

        bool operator==(const task& other)
        {
            return m_payload == other.m_payload;
        }

        //
        // Executes a callable on this scheduler once this task is finished and
        // returns a task that represents the callable.
        //
        // Calling .then() on the returned task will queue a task to run after the
        // callable is run.
        //
        template<typename SchedulerT, typename CallableT>
        auto then(SchedulerT& scheduler, cancellation& token, CallableT&& callable)
        {
            using traits = internal::callable_traits<CallableT, result_type>;
            using wrapper = internal::input_output_wrapper<result_type, error_type, traits::handles_expected::value>;

            static_assert(error_priority<error_type>::value <= error_priority<typename traits::error_propagation_type>::value,
                "The error of a parent task needs to be convertible to the error of a child task.");

            static_assert(std::is_same_v<typename expected_error_or<typename traits::input_type, error_type>::type, error_type>,
                "Continuation expected input parameter needs to use the same error type as the parent task");

            auto factory{ internal::make_task_factory(
                internal::make_work_payload<typename traits::expected_return_type::value_type, typename traits::error_propagation_type>(
                    [callable = wrapper::wrap_callable(std::forward<CallableT>(callable), token)]
                    (internal::base_task_payload* self) mutable noexcept
                    {
                        return callable(*static_cast<payload_t*>(self)->Result);
                    })
            ) };

            m_payload->create_continuation([&scheduler](auto&& c)
            {
                scheduler(std::forward<decltype(c)>(c));
            }, m_payload, std::move(factory.to_run.m_payload));

            return factory.to_return;
        }

    private:
        explicit task(payload_ptr payload)
            : m_payload{ std::move(payload) }
        {}

        template<typename OtherResultT, typename OtherErrorT>
        friend class task;

        friend class task_completion_source<ResultT, ErrorT>;

        template<typename OtherErrorT, typename OtherResultT>
        friend struct internal::task_factory;

        template<typename SchedulerT, typename CallableT>
        friend auto make_task(SchedulerT& scheduler, cancellation& token, CallableT&& callable)
            -> typename internal::task_factory<
                            typename internal::callable_traits<CallableT, void>::error_propagation_type,
                            typename internal::callable_traits<CallableT, void>::expected_return_type::value_type>::task_t;

        payload_ptr m_payload;
    };
}

namespace arcana
{
    template<typename ResultT, typename ErrorT>
    class task_completion_source
    {
        using payload_t = internal::task_payload_with_return<ResultT, ErrorT>;
        using payload_ptr = std::shared_ptr<payload_t>;

    public:
        using result_type = ResultT;
        using error_type = ErrorT;

        task_completion_source()
            : m_payload{ std::make_shared<payload_t>() }
        {}

        //
        // Completes the task this source represents.
        //
        void complete()
        {
            static_assert(std::is_same<ResultT, void>::value,
                "complete with no arguments can only be used with a void completion source");
            m_payload->complete(basic_expected<void, ErrorT>::make_valid());
        }

        //
        // Completes the task this source represents.
        //
        template<typename ValueT>
        void complete(ValueT&& value)
        {
            m_payload->complete(std::forward<ValueT>(value));
        }

        //
        // Returns whether or not the current source has already been completed.
        //
        bool completed() const
        {
            return m_payload->completed();
        }

        //
        // Converts this task_completion_source to a task object for consumers to use.
        //
        task<ResultT, ErrorT> as_task() const &
        {
            return task<ResultT, ErrorT>{ m_payload };
        }

        task<ResultT, ErrorT> as_task() &&
        {
            return task<ResultT, ErrorT>{ std::move(m_payload) };
        }

    private:
        explicit task_completion_source(std::shared_ptr<payload_t> payload)
            : m_payload{ std::move(payload) }
        {}

        friend class task<ResultT, ErrorT>;
        friend class abstract_task_completion_source;

        template<typename E, typename R>
        friend struct internal::task_factory;

        payload_ptr m_payload;
    };

    //
    // a type erased version of task_completion_source.
    //
    class abstract_task_completion_source
    {
        using payload_t = internal::base_task_payload;
        using payload_ptr = std::shared_ptr<payload_t>;

    public:
        abstract_task_completion_source()
            : m_payload{}
        {}

        template<typename T, typename E>
        explicit abstract_task_completion_source(const task_completion_source<T, E>& other)
            : m_payload{ other.m_payload }
        {}

        template<typename T, typename E>
        explicit abstract_task_completion_source(task_completion_source<T, E>&& other)
            : m_payload{ std::move(other.m_payload) }
        {}

        //
        // Returns whether or not the current source has already been completed.
        //
        bool completed() const
        {
            return m_payload->completed();
        }

        template<typename T, typename E>
        bool operator==(const task_completion_source<T, E>& other)
        {
            return m_payload == other.m_payload;
        }

        template<typename T, typename E>
        task_completion_source<T, E> unsafe_cast()
        {
            return task_completion_source<T, E>{ std::static_pointer_cast<internal::task_payload_with_return<T, E>>(m_payload) };
        }

    private:
        payload_ptr m_payload;
    };

    //
    // creates a task and queues it to run on the given scheduler
    //
    template<typename SchedulerT, typename CallableT>
    inline auto make_task(SchedulerT& scheduler, cancellation& token, CallableT&& callable)
        -> typename internal::task_factory<
                            typename internal::callable_traits<CallableT, void>::error_propagation_type,
                            typename internal::callable_traits<CallableT, void>::expected_return_type::value_type>::task_t
    {
        using traits = internal::callable_traits<CallableT, void>;
        using wrapper = internal::input_output_wrapper<void, typename traits::error_propagation_type, false>;

        auto factory{ internal::make_task_factory(
            internal::make_work_payload<typename traits::expected_return_type::value_type, typename traits::error_propagation_type>(
                [callable = wrapper::wrap_callable(std::forward<CallableT>(callable), token)]
                (internal::base_task_payload*) mutable noexcept
                {
                    return callable(basic_expected<void, typename traits::error_propagation_type>::make_valid());
                })
        ) };

        scheduler([to_run = std::move(factory.to_run)]
        {
            to_run.m_payload->run(nullptr);
        });

        return factory.to_return;
    }

    //
    // creates a completed task from the given result
    //
    template<typename ErrorT, typename ResultT>
    inline task<typename as_expected<ResultT, ErrorT>::value_type, ErrorT> task_from_result(ResultT&& value)
    {
        task_completion_source<typename as_expected<ResultT, ErrorT>::value_type, ErrorT> result;
        result.complete(std::forward<ResultT>(value));
        return result;
    }

    template<typename ErrorT>
    inline task<void, ErrorT> task_from_result()
    {
        task_completion_source<void, ErrorT> result;
        result.complete();
        return result;
    }

    template<typename ResultT, typename ErrorT>
    inline task<ResultT, std::error_code> task_from_error(const ErrorT& error)
    {
        task_completion_source<ResultT, std::error_code> result;
        result.complete(make_unexpected(make_error_code(error)));
        return result;
    }

    template<typename ResultT>
    inline task<ResultT, std::error_code> task_from_error(const std::error_code& error)
    {
        task_completion_source<ResultT, std::error_code> result;
        result.complete(make_unexpected(error));
        return result;
    }

    template<typename ResultT>
    inline task<ResultT, std::exception_ptr> task_from_error(const std::exception_ptr& error)
    {
        task_completion_source<ResultT, std::exception_ptr> result;
        result.complete(make_unexpected(error));
        return result;
    }

    template<typename ErrorT>
    inline task<void, ErrorT> when_all(gsl::span<task<void, ErrorT>> tasks)
    {
        if (tasks.empty())
        {
            return task_from_result<ErrorT>();
        }

        struct when_all_data
        {
            std::mutex mutex;
            size_t pendingCount;
            ErrorT error;
        };

        task_completion_source<void, ErrorT> result;
        auto data = std::make_shared<when_all_data>();
        data->pendingCount = tasks.size();

        for (task<void, ErrorT>& task : tasks)
        {
            task.then(inline_scheduler, cancellation::none(), [data, result](const basic_expected<void, ErrorT>& exp) mutable noexcept(std::is_same<ErrorT, std::error_code>::value) {
                bool last = false;
                {
                    std::lock_guard<std::mutex> guard{ data->mutex };

                    data->pendingCount -= 1;
                    last = data->pendingCount == 0;

                    if (exp.has_error() && !data->error)
                    {
                        // set the first error, as it might have cascaded
                        data->error = exp.error();
                    }
                }

                if (last) // we were the last task to complete
                {
                    if (data->error)
                    {
                        result.complete(make_unexpected(data->error));
                    }
                    else
                    {
                        result.complete();
                    }
                }
            });
        }

        return result;
    }

    template<typename T, typename ErrorT>
    inline task<std::vector<T>, ErrorT> when_all(gsl::span<task<T, ErrorT>> tasks)
    {
        if (tasks.empty())
        {
            return task_from_result<ErrorT, std::vector<T>>(std::vector<T>());
        }

        struct when_all_data
        {
            std::mutex mutex;
            size_t pendingCount;
            ErrorT error;
            std::vector<T> results;
        };

        task_completion_source<std::vector<T>, ErrorT> result;
        auto data = std::make_shared<when_all_data>();
        data->pendingCount = tasks.size();
        data->results.resize(tasks.size());

        //using forloop with index to be able to keep proper order of results
        for (auto idx = 0U; idx < data->results.size(); idx++)
        {
            tasks[idx].then(arcana::inline_scheduler, cancellation::none(), [data, result, idx](const basic_expected<T, ErrorT>& exp) mutable noexcept(std::is_same<ErrorT, std::error_code>::value) {
                bool last = false;
                {
                    std::lock_guard<std::mutex> guard{ data->mutex };

                    data->pendingCount -= 1;
                    last = data->pendingCount == 0;

                    if (exp.has_error() && !data->error)
                    {
                        // set the first error, as it might have cascaded
                        data->error = exp.error();
                    }
                    if (exp.has_value())
                    {
                        data->results[idx] = exp.value();
                    }
                }

                if (last) // we were the last task to complete
                {
                    if (data->error)
                    {
                        result.complete(make_unexpected(data->error));
                    }
                    else
                    {
                        result.complete(data->results);
                    }
                }
            });
        }

        return result;
    }

    template<typename ErrorT, typename... ArgTs>
    inline task<std::tuple<typename arcana::void_passthrough<ArgTs>::type...>, ErrorT> when_all(task<ArgTs, ErrorT>... tasks)
    {
        using void_passthrough_tuple = std::tuple<typename arcana::void_passthrough<ArgTs>::type...>;

        struct when_all_data
        {
            std::mutex mutex;
            int pending;
            ErrorT error;
            void_passthrough_tuple results;
        };
  
        task_completion_source<void_passthrough_tuple, ErrorT> result;

        auto data = std::make_shared<when_all_data>();
        data->pending = std::tuple_size<void_passthrough_tuple>::value;

        std::tuple<task<ArgTs, ErrorT>&...> taskrefs = std::make_tuple(std::ref(tasks)...);

        iterate_tuple(taskrefs, [&](auto& task, auto idx) {
            using task_t = std::remove_reference_t<decltype(task)>;

            task.then(inline_scheduler,
                cancellation::none(),
                [data, result](const basic_expected<typename task_t::result_type, ErrorT>& exp) mutable noexcept {
                bool last = false;
                {
                    std::lock_guard<std::mutex> guard{ data->mutex };

                    data->pending -= 1;
                    last = data->pending == 0;

                    internal::write_expected_to_tuple<decltype(idx)::value, ErrorT>(data->results, exp);
                    
                    if (exp.has_error() && !data->error)
                    {
                        // set the first error, as it might have cascaded
                        data->error = exp.error();
                    }
                }

                if (last) // we were the last task to complete
                {
                    if (data->error)
                    {
                        result.complete(make_unexpected(data->error));
                    }
                    else
                    {
                        result.complete(std::move(data->results));
                    }
                }

                return basic_expected<void, ErrorT>::make_valid();
            });
        });
        return result;
    }
}
