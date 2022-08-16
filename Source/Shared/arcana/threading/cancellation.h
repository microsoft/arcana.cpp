#pragma once

#include "arcana/containers/ticketed_collection.h"

#include <gsl/gsl>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <vector>

namespace arcana
{
    namespace internal
    {
        class cancellation_impl
        {
            using collection = ticketed_collection<std::function<void()>>;

        public:
            using ticket = collection::ticket;
            using ticket_scope = collection::ticket_scope;

            template<typename CallableT>
            ticket add_cancellation_requested_listener(CallableT&& callback, std::function<void()>& copied)
            {
                return internal_add_listener(m_cancelRequestedListeners, std::forward<CallableT>(callback), copied);
            }

            template<typename CallableT>
            ticket add_cancellation_completed_listener(CallableT&& callback, std::function<void()>& copied)
            {
                return internal_add_listener(m_cancelCompletedListeners, std::forward<CallableT>(callback), copied);
            }

            void unsafe_cancel()
            {
                {
                    std::scoped_lock lock{ m_mutex };

                    if (m_cancelStarted == true)
                    {
                        return;
                    }

                    m_cancelStarted = true;

                    if (m_pins == 0)
                    {
                        m_cancelFinished = true;
                    }
                }

                if (m_cancelStarted)
                {
                    signal_cancellation(m_cancelRequestedListeners);
                }

                if (m_cancelFinished)
                {
                    signal_cancellation(m_cancelCompletedListeners);
                }
            }

            bool cancelled() const
            {
                std::scoped_lock lock{ m_mutex };
                return m_cancelStarted;
            }

            auto pin()
            {
                std::scoped_lock lock{ m_mutex };
                if (m_cancelStarted)
                {
                    return std::optional<gsl::final_action<std::function<void()>>>{};
                }
                else
                {
                    ++m_pins;
                    return std::optional{gsl::finally(std::function<void()>{[this]
                    {
                        assert(!m_cancelFinished);
                        {
                            std::scoped_lock lock{ m_mutex };
                            m_cancelFinished = --m_pins == 0 && m_cancelStarted;
                        }
                        if (m_cancelFinished)
                        {
                            signal_cancellation(m_cancelCompletedListeners);
                        }
                    }})};
                }
            }

        private:
            bool m_cancelStarted{ false };
            bool m_cancelFinished{ false };
            collection m_cancelRequestedListeners;
            collection m_cancelCompletedListeners;
            mutable std::mutex m_mutex;

            size_t m_pins{ 0 };

            void signal_cancellation(collection& listeners)
            {
                std::vector<std::function<void()>> listenersCopy;
                {
                    std::scoped_lock lock{ m_mutex };
                    listenersCopy.reserve(listeners.size());
                    std::copy(listeners.begin(), listeners.end(), std::back_inserter(listenersCopy));
                }

                // We want to signal cancellation in reverse order
                // so that if a parent function adds a listener
                // then a child function does the same, the child
                // cancellation runs first. This avoids ownership
                // semantic issues.
                for (auto itr = listenersCopy.rbegin(); itr != listenersCopy.rend(); ++itr)
                {
                    (*itr)();
                }
            }

            template<typename CallableT>
            ticket internal_add_listener(collection& listeners, CallableT&& callback, std::function<void()>& copied)
            {
                std::scoped_lock<std::mutex> lock{ m_mutex };

                if (m_cancelFinished)
                {
                    copied = std::forward<CallableT>(callback);
                    return listeners.insert(copied, m_mutex);
                }
                else
                {
                    return listeners.insert(std::forward<CallableT>(callback), m_mutex);
                }
            }
        };
    }

    class cancellation
    {
    public:
        using ticket = internal::cancellation_impl::ticket;
        using ticket_scope = internal::cancellation_impl::ticket_scope;

        bool cancelled() const
        {
            if (this == &none())
                return false;

            return m_impl->cancelled();
        }

        void throw_if_cancellation_requested()
        {
            if (cancelled())
            {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
        }

        /*
            Adds a callback that will get called when cancellation is requested. The callback
            will be called synchronously if cancellation has already happened.
        */
        template<typename CallableT>
        ticket add_cancellation_requested_listener(CallableT&& callback)
        {
            if (this == &none())
                return ticket{ [] {} };

            std::function<void()> copied;
            ticket result{ m_impl->add_cancellation_requested_listener(callback, copied) };

            if (copied)
                copied();

            return result;
        }

        /*
            Adds a callback that will get called when cancellation is completed -- i.e., when
            no work "guarded" by the cancellation will ever be done again. The callback
            will be called synchronously if cancellation has already completed.
        */
        template<typename CallableT>
        ticket add_cancellation_completed_listener(CallableT&& callback)
        {
            if (this == &none())
                return ticket{ [] {} };

            std::function<void()> copied;
            ticket result{ m_impl->add_cancellation_completed_listener(callback, copied) };

            if (copied)
                copied();

            return result;
        }

        auto pin()
        {
            return m_impl->pin();
        }

        static cancellation& none();

    private:
        friend class cancellation_source;
        std::shared_ptr<internal::cancellation_impl> m_impl;

        cancellation(std::shared_ptr<internal::cancellation_impl> ptr)
            : m_impl{ std::move(ptr) }
        {
        }
    };

    class cancellation_source : public cancellation
    {
    public:
        cancellation_source()
            : cancellation{ std::make_shared<internal::cancellation_impl>() }
        {
        }

        cancellation_source(const cancellation_source&) = delete;
        cancellation_source(cancellation_source&&) = delete;

        void cancel(bool blockUntilCompleted = false)
        {
            std::optional<std::promise<void>> promise{};
            std::optional<cancellation::ticket> ticket{};
            if (blockUntilCompleted)
            {
                promise.emplace();
                ticket.emplace(add_cancellation_completed_listener([&promise]() {
                    promise.value().set_value();
                }));
            }

            m_impl->unsafe_cancel();

            if (promise)
            {
                promise.value().get_future().wait();
            }
        }
    };

    namespace internal::no_destroy_cancellation
    {
        // To address a problem with static globals recognized as the same as in a 
        // standards proposal, we adopt a workaround described in the proposal.
        // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1247r0.html
        template <class T>
        class no_destroy
        {
            alignas(T) unsigned char data_[sizeof(T)];

        public:
            template <class... Ts>
            no_destroy(Ts&&... ts)
            {
                new (data_)T(std::forward<Ts>(ts)...);
            }

            T &get()
            {
                return *reinterpret_cast<T *>(data_);
            }
        };

        inline no_destroy<cancellation_source> none{};
    }

    inline cancellation& cancellation::none()
    {
        return internal::no_destroy_cancellation::none.get();
    }
}
