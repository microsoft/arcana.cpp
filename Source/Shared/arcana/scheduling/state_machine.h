#pragma once

#include "state_machine_state.h"

#include "arcana/threading/cancellation.h"
#include "arcana/threading/task.h"

#include <unordered_map>

namespace arcana
{
    class state_machine_observer;

    //
    // This class represents a state machine, and can be used to control which
    // state it is currently in. Consumers should be passed a state_machine_observer
    // in order to run logic when this state machine has reached a particular state.
    //
    class state_machine_driver
    {
    public:
        //
        //  Moves the state machine into the state.
        //
        template<typename T>
        task<T, std::error_code> move_to(state_machine_state<T>& state, cancellation& cancel)
        {
            completions stateTasks{};

            {
                std::lock_guard<std::mutex> guard{ m_mutex };
                stateTasks = fetch_state(state);
            }

            stateTasks.StateEntered->complete(expected<void, std::error_code>::make_valid());

            std::shared_ptr<cancellation_data> listener = std::make_shared<cancellation_data>(cancel.add_cancellation_requested_listener([this, &state]
            {
                cancel_exit(state);
            }));

            return stateTasks.StateExited.unsafe_cast<T, std::error_code>().as_task()
                .then(inline_scheduler, cancellation::none(),
                    [listener = std::move(listener)](const arcana::expected<T, std::error_code>& result) mutable noexcept
                    {
                        listener.reset();
                        return result;
                    });
        }

    private:
        struct cancellation_data
        {
            cancellation::ticket ticket;

            cancellation_data(cancellation::ticket&& ticket)
                : ticket{ std::move(ticket) }
            {}
        };

        template<typename T>
        task<void, std::error_code> enter(state_machine_state<T>& state, cancellation& cancel)
        {
            completions stateTasks{};

            {
                std::lock_guard<std::mutex> guard{ m_mutex };

                auto& completion = fetch_state(state);

                assert(!completion.WorkPending && "tried entering a node that is already entered");
                completion.WorkPending = true;

                stateTasks = completion;
            }

            std::shared_ptr<cancellation_data> listener = std::make_shared<cancellation_data>(cancel.add_cancellation_requested_listener([this, &state]
            {
                cancel_enter(state);
            }));

            return stateTasks.StateEntered->as_task()
                .then(inline_scheduler, cancellation::none(), 
                    [listener = std::move(listener)](const arcana::expected<void, std::error_code>& result) mutable noexcept
                    {
                        listener.reset();
                        return result;
                    });
        }

        void cancel_enter(abstract_state_machine_state& state)
        {
            std::optional<task_completion_source<void, std::error_code>> enter;
            {
                std::lock_guard<std::mutex> guard{ m_mutex };

                auto found = m_states.find(&state);
                if (found == m_states.end())
                {
                    return;
                }

                if (found->second.StateEntered->completed())
                {
                    return;
                }

                enter = found->second.StateEntered;
                m_states.erase(found);
            }

            enter->complete(make_unexpected(std::make_error_code(std::errc::operation_canceled)));
        }

        template<typename T>
        void cancel_exit(state_machine_state<T>& state)
        {
            task_completion_source<T, std::error_code> exit;
            {
                std::lock_guard<std::mutex> guard{ m_mutex };

                auto found = m_states.find(&state);
                if (found == m_states.end())
                {
                    return;
                }

                if (found->second.StateExited.completed())
                {
                    return;
                }

                exit = found->second.StateExited.template unsafe_cast<T, std::error_code>();
                m_states.erase(found);
            }

            exit.complete(make_unexpected(std::make_error_code(std::errc::operation_canceled)));
        }

        template<typename T>
        void exit(state_machine_state<T>& state, const T& data)
        {
            std::optional<task_completion_source<T, std::error_code>> task;

            {
                std::lock_guard<std::mutex> guard{ m_mutex };

                auto found = m_states.find(&state);
                if (found == m_states.end())
                {
                    return;
                }

                // remove the state from m_states as
                // the completion of a StateExited can re-enter
                // the same states when and in that case
                // we want the when to create a new completions object.
                task = std::move(found->second.StateExited).template unsafe_cast<T, std::error_code>();
                m_states.erase(found);
            }

            task->complete(data);
        }

        void exit(state_machine_state<void>& state)
        {
            std::optional<task_completion_source<void, std::error_code>> task;

            {
                std::lock_guard<std::mutex> guard{ m_mutex };

                auto found = m_states.find(&state);
                if (found == m_states.end())
                {
                    return;
                }

                // remove the state from m_states as
                // the completion of a StateExited can re-enter
                // the same states when and in that case
                // we want the when to create a new completions object.
                task = std::move(found->second.StateExited).unsafe_cast<void, std::error_code>();
                m_states.erase(found);
            }

            task->complete(expected<void, std::error_code>::make_valid());
        }

        struct completions
        {
            std::optional<task_completion_source<void, std::error_code>> StateEntered;
            abstract_task_completion_source StateExited{};
            bool WorkPending{ false };
        };

        template<typename T>
        completions& fetch_state(state_machine_state<T>& state)
        {
            auto existing = m_states.find(&state);
            if (existing == m_states.end())
            {
                auto inserted = m_states
                                    .insert(
                                        { &state,
                                          completions{ task_completion_source<void, std::error_code>{},
                                                       abstract_task_completion_source{ task_completion_source<T, std::error_code>{} },
                                                       false } })
                                    .first;

                return inserted->second;
            }
            else
            {
                return existing->second;
            }
        }

        mutable std::mutex m_mutex;
        std::unordered_map<abstract_state_machine_state*, completions> m_states;

        friend class state_machine_observer;
    };

    //
    // This class represents a read-only view on a state machine, and can
    // be used to run code when the state machine has entered a particular state.
    // Read-only meaning it should be used in order to run code during states,
    // but can't directly influence the state machine.
    //
    class state_machine_observer
    {
    public:
        explicit state_machine_observer(state_machine_driver& driver)
            : m_driver{ driver }
        {}

        //
        // Runs the callable when the state machine reaches the specified state
        // on the dispatcher provided.
        //
        template<typename ResultT, typename DispatcherT, typename CallableT>
        auto on(state_machine_state<ResultT>& state,
                DispatcherT& dispatcher,
                cancellation& cancel,
                CallableT&& callable)
        {
            return on_worker<ResultT>::on(m_driver, state, dispatcher, cancel, std::forward<CallableT>(callable));
        }

    private:
        template<typename ResultT>
        struct on_worker
        {
            template<typename DispatcherT, typename CallableT>
            static auto on(state_machine_driver& driver,
                           state_machine_state<ResultT>& state,
                           DispatcherT& dispatcher,
                           cancellation& cancel,
                           CallableT&& callable)
            {
                struct pending_data
                {
                    ResultT data{};
                };

                auto node_data = std::make_shared<pending_data>();

                ResultT* dataptr = &node_data->data;

                using callable_traits = internal::callable_traits<CallableT, ResultT>;
                static_assert(std::is_same_v<typename callable_traits::error_propagation_type, std::error_code>, "state machine only supports error_codes for now");

                auto work = [ callable = std::forward<CallableT>(callable), dataptr ]() noexcept
                {
                    return callable(*dataptr);
                };

                return driver.enter(state, cancel)
                    .then(dispatcher, cancel, std::move(work))
                    .then(inline_scheduler,
                          cancellation::none(),
                          [&, node_data = std::move(node_data) ](const typename callable_traits::expected_return_type& result) mutable noexcept {
                              ResultT data = std::move(node_data->data);
                              // remove ourselves as a cancellation listener, as we were
                              // only listening in order to avoid getting stuck on enter.
                              node_data.reset();

                              driver.exit(state, data);

                              return result;
                          });
            }
        };

        state_machine_driver& m_driver;
    };

    template<>
    struct state_machine_observer::on_worker<void>
    {
        template<typename DispatcherT, typename CallableT>
        static auto on(state_machine_driver& driver,
                       state_machine_state<void>& state,
                       DispatcherT& dispatcher,
                       cancellation& cancel,
                       CallableT&& callable)
        {
            using callable_traits = internal::callable_traits<CallableT, void>;

            static_assert(std::is_same_v<typename callable_traits::error_propagation_type, std::error_code>, "state machine only supports error_codes for now");

            return driver.enter(state, cancel)
                .then(dispatcher, cancel, std::move(callable))
                .then(inline_scheduler,
                      cancellation::none(),
                      [&driver, &state](const typename callable_traits::expected_return_type& result) mutable noexcept {

                          driver.exit(state);

                          return result;
                      });
        }
    };
}
