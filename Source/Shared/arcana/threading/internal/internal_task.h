#pragma once

#include "callable_traits.h"

#include "arcana/functional/inplace_function.h"
#include "arcana/type_traits.h"

#include <mutex>
#include <optional>
#include <thread>
#include <variant>

namespace arcana
{
    template<typename ResultT, typename ErrorT>
    class task;

    namespace internal
    {
        struct base_task_payload
        {
        public:
            using work_function_t = void(*)(base_task_payload& payload, base_task_payload* parent);

            //
            // A task has 0-n continuations that run once it's done and that take
            // the result of the task as input parameters.
            //
            // A continuation doesn't necessarily run on the same scheduler as the task
            // it depends on. Since we need to keep these continuations in a homogeneous container
            // we type erase the scheduler by creating a callable that queues the run
            // method of the continuation on the right scheduler.
            //
            // We keep a weak_ptr to the parent because before the parent has run
            // it's holding on to the payload which would create a circular shared_ptr reference.
            // Once we queue the continuation we lock the weak_ptr in order to keep the
            // parent alive long enough for us to run and read its result. This doesn't create a circular
            // reference because it passes the ownership to the lambda that runs the continuation.
            //
            // The continuation task is the task payload that represents the task we need to run
            // when the previous one has been run.
            // 
            struct continuation_payload
            {
                using queue_function = stdext::inplace_function<void(), 2 * sizeof(std::shared_ptr<void>)>;
                using scheduling_function = stdext::inplace_function<void(queue_function&&), sizeof(intptr_t)>;

                explicit operator bool() const noexcept
                {
                    return continuation != nullptr;
                }

                continuation_payload() = default;

                continuation_payload(const scheduling_function& schedulingFunction, std::weak_ptr<base_task_payload> parentArg, std::shared_ptr<base_task_payload> continuationArg)
                    : parent{ std::move(parentArg) }
                    , continuation{ std::move(continuationArg) }
                    , schedulingFunction{ schedulingFunction }
                {
                    assert(parent.lock() && "parent of a continuation can't be null");
                }

                void reparent(const std::shared_ptr<base_task_payload>& newParent)
                {
                    assert(newParent && "tried reparenting with a null parent");
                    parent = newParent;
                }

                void run()
                {
                    assert(parent.lock() && "parent of a continuation can't be null");

                    schedulingFunction([shared = parent.lock(), continuation = std::move(continuation)]
                    {
                        assert(shared.get() && "parent of a continuation can't be null");

                        continuation->run(shared.get());
                    });
                }

            private:
                std::weak_ptr<base_task_payload> parent;
                std::shared_ptr<base_task_payload> continuation;

                scheduling_function schedulingFunction;
            };

            base_task_payload(work_function_t work)
                : m_work{ work }
            {}

            bool completed() const
            {
                return m_completed;
            }

            bool is_task_completion_source() const
            {
                return m_work == nullptr;
            }

            void run(base_task_payload* parent)
            {
                if (m_work != nullptr)
                {
                    m_work(*this, parent);
                }

                do_completion();
            }

            void create_continuation(
                const continuation_payload::scheduling_function& schedulingFunction,
                std::weak_ptr<base_task_payload> self,
                std::shared_ptr<base_task_payload> continuation)
            {
                add_continuation(continuation_payload{ schedulingFunction, std::move(self), std::move(continuation) });
            }

            static void collapse_left_into_right(base_task_payload& left, const std::shared_ptr<base_task_payload>& right)
            {
                auto continuations = left.cannibalize(right);
                right->add_continuations(continuation_span(continuations), right);
            }

            void complete()
            {
                do_completion();
            }

        private:
            std::variant<continuation_payload, std::vector<continuation_payload>> cannibalize(std::shared_ptr<base_task_payload> taskRedirect)
            {
                std::lock_guard<std::mutex> guard{ m_mutex };

                if (m_completed)
                    throw std::runtime_error("tried to complete a task twice");

                auto complete = gsl::finally([this] { m_completed = true; });

                m_taskRedirect = std::move(taskRedirect);

                // we need to clear the continuation once it's done because it
                // holds a reference to the payload during the transition period
                // and if we don't we'd create a circular shared_ptr reference and never destroy
                // the payload instances.
                return std::move(m_continuation);
            }

            static gsl::span<continuation_payload> continuation_span(std::variant<continuation_payload, std::vector<continuation_payload>>& either)
            {
                if (auto vect = std::get_if<1>(&either))
                {
                    return gsl::make_span(*vect);
                }
                else if (auto& solo = std::get<0>(either); solo) // make sure the continuation_payload has a value
                {
                    return gsl::make_span(&solo, 1);
                }
                else // if the single continuation_payload isn't set return the empty span
                {
                    return {};
                }
            }

            void add_continuation(continuation_payload&& continuation)
            {
                add_continuations(gsl::make_span<continuation_payload>(&continuation, 1));
            }

            void add_continuations(gsl::span<continuation_payload> continuations, const std::shared_ptr<base_task_payload>& self)
            {
                assert(self.get() == this && "when adding continuations through cannibalization we need to reparent the continuations to ourselves");

                if (continuations.empty())
                    return;

                for (auto& continuation : continuations)
                    continuation.reparent(self);

                add_continuations(continuations);
            }

            void add_continuations(gsl::span<continuation_payload> continuations)
            {
                if (continuations.empty())
                    return;

                bool runit = false;

                {
                    std::lock_guard<std::mutex> guard{ m_mutex };

                    if (m_taskRedirect)
                    {
                        m_taskRedirect->add_continuations(continuations, m_taskRedirect);
                    }
                    else
                    {
                        if (m_completed)
                        {
                            runit = true;
                        }
                        else
                        {
                            internal_add_continuations(continuations);
                        }
                    }
                }

                if (runit)
                {
                    for (auto& continuation : continuations)
                        continuation.run();
                }
            }

            void do_completion()
            {
                std::variant<continuation_payload, std::vector<continuation_payload>> continuation = cannibalize(nullptr);

                for (auto& callable : continuation_span(continuation))
                    callable.run();
            }

            void internal_add_continuations(continuation_payload&& continuation)
            {
                internal_add_continuations(gsl::make_span(&continuation, 1));
            }

            void internal_add_continuations(gsl::span<continuation_payload> continuations)
            {
                if (m_completed)
                    throw std::runtime_error("already specified a continuation for the current task");

                assert(!continuations.empty() && "we shouldn't be calling this with an empty continuation set");

                if (auto vect = std::get_if<1>(&m_continuation))
                {
                    vect->insert(
                        vect->end(), std::make_move_iterator(continuations.begin()), std::make_move_iterator(continuations.end()));
                }
                else if (!std::get<0>(m_continuation) && continuations.size() == 1)
                {
                    std::get<0>(m_continuation) = std::move(continuations[0]);
                }
                else
                {
                    // create a vector with the existing continuation (if there is one)
                    // then append the new ones.
                    if (std::get<0>(m_continuation))
                    {
                        m_continuation = std::vector<continuation_payload>{ std::move(std::get<0>(m_continuation)) };

                        std::get<1>(m_continuation).insert(
                            std::get<1>(m_continuation).end(),
                            std::make_move_iterator(continuations.begin()), std::make_move_iterator(continuations.end()));
                    }
                    else
                    {
                        m_continuation = std::vector<continuation_payload>(
                            std::make_move_iterator(continuations.begin()), std::make_move_iterator(continuations.end()));
                    }
                }
            }

        protected:
            // TODO make sure we don't have huge gaps in the object layout
            std::mutex m_mutex;

        private:
            bool m_completed = false;
            work_function_t m_work = nullptr;
            std::variant<continuation_payload, std::vector<continuation_payload>> m_continuation{ continuation_payload{} };

            // when unwrapping multiple tasks of tasks we don't want to
            // create unbounded continuation chains. Which means we cannibalize
            // the top level task_completion_source and add all its continuations
            // to the task that gets returned from the async method. Once we cannibalize
            // the original task still exists, so if someone tries to add itself as a
            // continuation it won't ever get called or won't have the right result.
            // In order to get around that we set task redirect (like a forwarding address or an http 302 redirect)
            // which is the task returned by the async method that now represents the actual task that
            // will eventually get completed and contain the result that was meant for the
            // original task_completion_source. The m_taskRedirect task is then where we add the continuation instead
            // of the task_completion_source.
            std::shared_ptr<base_task_payload> m_taskRedirect;
        };

        template<typename ResultT, typename ErrorT>
        struct task_payload_with_return : base_task_payload
        {
            std::optional<basic_expected<ResultT, ErrorT>> Result;

            task_payload_with_return()
                : base_task_payload{ nullptr }
            {}

            task_payload_with_return(work_function_t work)
                : base_task_payload{ work }
            {}

            void complete(basic_expected<ResultT, ErrorT>&& result)
            {
                Result = std::move(result);

                base_task_payload::complete();
            }

            void complete(const basic_expected<ResultT, ErrorT>& result)
            {
                Result = result;

                base_task_payload::complete();
            }
        };

        template<typename ResultT, typename ErrorT, size_t WorkSize>
        struct task_payload_with_work : public task_payload_with_return<ResultT, ErrorT>
        {
            using WorkT = stdext::inplace_function<basic_expected<ResultT, ErrorT>(base_task_payload*), WorkSize, alignof(std::max_align_t), false>;
            WorkT Work;

            task_payload_with_work(WorkT work)
                : task_payload_with_return<ResultT, ErrorT>{ &do_work }
                , Work{ std::move(work) }
            {}

            static void do_work(base_task_payload& base, base_task_payload* baseParent)
            {
                auto& self = static_cast<task_payload_with_work&>(base);

                self.Result = self.Work(baseParent);
                self.Work = {}; // clear the work to destroy the callable
            }
        };

        template<typename ResultT, typename ErrorT, typename CallableT>
        std::shared_ptr<task_payload_with_return<ResultT, ErrorT>> make_work_payload(CallableT&& callable)
        {
            return std::make_shared<task_payload_with_work<ResultT, ErrorT, sizeof(callable)>>(std::forward<CallableT>(callable));
        }

        //
        // task_factory is responsible for creating the task objects on continuations.
        // The task_factory<task> specialization is used to unwrap task<task> for callers.
        //
        template<typename ErrorT, typename ReturnT>
        struct task_factory
        {
            using task_t = task<ReturnT, ErrorT>;

            task_factory(std::shared_ptr<task_payload_with_return<ReturnT, ErrorT>> payload)
                : to_run{ std::move(payload) }
                , to_return{ to_run }
            {}

            task_t to_run;
            task_t to_return;
        };

        template<typename ErrorT, typename TaskReturnT, typename TaskErrorT>
        struct task_factory<ErrorT, task<TaskReturnT, TaskErrorT>>
        {
            using largest_error = typename largest_error<ErrorT, TaskErrorT>::type;

            using task_t = task<TaskReturnT, largest_error>;

            task_factory(std::shared_ptr<task_payload_with_return<task<TaskReturnT, TaskErrorT>, ErrorT>> payload)
                : to_run{ std::move(payload) }
            {
                task_completion_source<TaskReturnT, largest_error> source;

                to_run.then(inline_scheduler, cancellation::none(), [source](const basic_expected<task<TaskReturnT, TaskErrorT>, ErrorT>& result) mutable noexcept {
                    if (result.has_error())
                    {
                        source.complete(make_unexpected(result.error()));
                    }
                    else
                    {
                        // Here when the result is also a task_completion_source
                        // we can collapse them to implement a sort of task tail recursion
                        // to remove unbounded task_completion_source chains.
                        // We achieve this by pulling out all the continuations from our
                        // task_completion_source which was a stand in for the task,
                        // and putting them on the actual task it was representing.
                        //
                        // We then set the forwarding task on the source in case
                        // someone is keeping it around and wants to put a continuation on
                        // it after the fact (in the cannibalize method). This ensures that
                        // anyone calling .then() on the stand-in task_completion_source
                        // we be added as a continuation to the right task.
                        //
                        // Then when we add the continuations to the task, we update their parent
                        // tasks (or where they get their results from) to the new task that was
                        // created in the callable. This ensures that the continuations use
                        // the result from the inner most task on completion which stores the actual result
                        // of the whole chain.
                        //
                        // The downside of the forwarding task is that if you keep a top level
                        // stand-in completion source around and call .then() on it after unwrapping
                        // you'll have to walk a potentially unbounded chain of forwarding tasks.
                        // Thankfully in all our scenarios that use infinite recursive tasks we immediately
                        // add a continuation to it and then await cancellation when we're done.
                        //
                        // One can think of this system like a platformer where the character
                        // is running on a platform that is advancing using pieces from its tail.
                        //
                        //   step 1:     step 2:   step 3:  (repeat)
                        //      __o        __o        __o 
                        //    _ \<_      _ \<_      _ \<_ 
                        //   (_)/(_)    (_)/(_)    (_)/(_)
                        //   a b c d     b c d     b c d a
                        //                 a
                        //

                        base_task_payload::collapse_left_into_right(*source.m_payload, result.value().m_payload);
                    }

                    return basic_expected<void, TaskErrorT>::make_valid();
                });

                to_return = std::move(source);
            }

            task<task<TaskReturnT, TaskErrorT>, ErrorT> to_run;
            task_t to_return;
        };

        template<typename ErrorT, typename ResultT>
        inline auto make_task_factory(std::shared_ptr<task_payload_with_return<ResultT, ErrorT>> payload)
        {
            return task_factory<ErrorT, ResultT>( std::move(payload ) );
        }

        //
        // The output_wrapper serves to invoke the function and convert its output to
        // an expected<ResultT, ErrorT>. It also ensures that exceptions
        // thrown if the error_type is std::exception_ptr are caught and returned.
        //
        template<typename ResultT, typename ErrorT>
        struct output_wrapper
        {
            template<typename CallableT, typename... InputT>
            static typename as_expected<ResultT, ErrorT>::type invoke(CallableT&& callable, InputT&&... input) noexcept
            {
                if constexpr (std::is_same<ErrorT, std::error_code>::value)
                {
                    return callable(std::forward<InputT>(input)...);
                }
                else
                {
                    try
                    {
                        return callable(std::forward<InputT>(input)...);
                    }
                    catch (...)
                    {
                        return make_unexpected(std::current_exception());
                    }
                }
            }
        };

        template<typename ErrorT>
        struct output_wrapper<void, ErrorT>
        {
            template<typename CallableT, typename... InputT>
            static basic_expected<void, ErrorT> invoke(CallableT&& callable, InputT&&... input) noexcept
            {
                if constexpr (std::is_same<ErrorT, std::error_code>::value)
                {
                    callable(std::forward<InputT>(input)...);

                    return basic_expected<void, ErrorT>::make_valid();
                }
                else
                {
                    try
                    {
                        callable(std::forward<InputT>(input)...);

                        return basic_expected<void, ErrorT>::make_valid();
                    }
                    catch (...)
                    {
                        return make_unexpected(std::current_exception());
                    }
                }
            }
        };

        //
        // The input_output_wrapper types job is to grab an arbitrary lamba and adapt it
        // so that it returns an expected<ReturnT> and takes an expected<InputT>.
        //
        // While doing so it also adds the helper logic which takes care of cancellation
        // and error states for methods that use cancellation tokens and receive InputT
        // as a parameter directly.
        //
        template<typename InputT, typename InputErrorT, bool HandlesExpected>
        struct input_output_wrapper;

        template<typename InputT, typename InputErrorT>
        struct input_output_wrapper<InputT, InputErrorT, true /*HandlesExpected*/>
        {
            template<typename CallableT>
            static auto wrap_callable(CallableT&& callable, cancellation& cancel)
            {
                using traits = callable_traits<CallableT, InputT>;

                return[callable = std::forward<CallableT>(callable), &cancel](const basic_expected<InputT, InputErrorT>& input) mutable noexcept
                {
                    if (cancel.cancelled())
                        return typename traits::expected_return_type{ make_unexpected(std::errc::operation_canceled) };

                    return output_wrapper<typename traits::return_type, typename traits::error_propagation_type>::invoke(callable, input);
                };
            }
        };

        template<typename InputT, typename InputErrorT>
        struct input_output_wrapper<InputT, InputErrorT, false /*HandlesExpected*/>
        {
            template<typename CallableT>
            static auto wrap_callable(CallableT&& callable, cancellation& cancel)
            {
                using traits = callable_traits<CallableT, InputT>;

                return[callable = std::forward<CallableT>(callable), &cancel](const basic_expected<InputT, InputErrorT>& input) mutable noexcept
                {
                    // Here the callable doesn't handle expected<> which means we don't have to invoke
                    // it if the previous task error'd out or its cancellation token is set.
                    if (input.has_error())
                        return typename traits::expected_return_type{ make_unexpected(input.error()) };

                    if (cancel.cancelled())
                        return typename traits::expected_return_type{ make_unexpected(std::errc::operation_canceled) };

                    return output_wrapper<typename traits::return_type, typename traits::error_propagation_type>::invoke(callable, input.value());
                };
            }
        };

        template<typename InputErrorT>
        struct input_output_wrapper<void, InputErrorT, false /*HandlesExpected*/>
        {
            template<typename CallableT>
            static auto wrap_callable(CallableT&& callable, cancellation& cancel)
            {
                using traits = callable_traits<CallableT, void>;

                return[callable = std::forward<CallableT>(callable), &cancel](const basic_expected<void, InputErrorT>& input) mutable noexcept
                {
                    // Here the callable doesn't handle expected<> which means we don't have to invoke
                    // it if the previous task error'd out or its cancellation token is set.
                    if (input.has_error())
                        return typename traits::expected_return_type{ make_unexpected(input.error()) };

                    if (cancel.cancelled())
                        return typename traits::expected_return_type{ make_unexpected(std::errc::operation_canceled) };

                    return output_wrapper<typename traits::return_type, typename traits::error_propagation_type>::invoke(callable);
                };
            }
        };

        template<size_t IndexV, typename ErrorT, typename ExpectedT, typename... TupleT>
        void write_expected_to_tuple(std::tuple<TupleT...>& tuple, const basic_expected<ExpectedT, ErrorT>& expected)
        {
            if (expected)
            {
                std::get<IndexV>(tuple) = expected.value();
            }
        }

        template<size_t IndexV, typename ErrorT, typename... TupleT>
        void write_expected_to_tuple(std::tuple<TupleT...>&, const basic_expected<void, ErrorT>&)
        {
        }
    }
}
