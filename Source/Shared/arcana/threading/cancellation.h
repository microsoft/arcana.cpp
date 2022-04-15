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
            ticket add_listener(CallableT&& callback, std::function<void()>& copied)
            {
                std::scoped_lock<std::mutex> lock{ m_mutex };

                if (m_cancelFinished)
                {
                    copied = std::forward<CallableT>(callback);
                    return m_listeners.insert(copied, m_mutex);
                }
                else
                {
                    return m_listeners.insert(std::forward<CallableT>(callback), m_mutex);
                }
            }

            void unsafe_cancel()
            {
                bool finishCancellation = false;
                {
                    std::scoped_lock lock{ m_mutex };

                    if (m_cancelStarted == true)
                    {
                        return;
                    }

                    m_cancelStarted = true;

                    if (m_pins == 0)
                    {
                        finishCancellation = true;
                    }
                }

                if (finishCancellation)
                {
                    finish_cancellation();
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
                        bool finishCancellation = false;
                        {
                            std::scoped_lock lock{ m_mutex };
                            finishCancellation = --m_pins == 0 && m_cancelStarted;
                        }
                        if (finishCancellation)
                        {
                            finish_cancellation();
                        }
                    }})};
                }
            }

        private:
            bool m_cancelStarted{ false };
            bool m_cancelFinished{ false };
            mutable std::mutex m_mutex;
            collection m_listeners;

            size_t m_pins{ 0 };

            void finish_cancellation()
            {
                std::vector<std::function<void()>> listeners;
                {
                    std::scoped_lock lock{ m_mutex };
                    assert(!m_cancelFinished);
                    m_cancelFinished = true;
                    listeners.reserve(m_listeners.size());
                    std::copy(m_listeners.begin(), m_listeners.end(), std::back_inserter(listeners));
                }

                // We want to signal cancellation in reverse order
                // so that if a parent function adds a listener
                // then a child function does the same, the child
                // cancellation runs first. This avoids ownership
                // semantic issues.
                for (auto itr = listeners.rbegin(); itr != listeners.rend(); ++itr)
                {
                    (*itr)();
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
            Adds a callback that will get called on cancellation. You cancellation
            will get called synchronously if cancellation has already happened.
        */
        template<typename CallableT>
        ticket add_listener(CallableT&& callback)
        {
            if (this == &none())
                return ticket{ [] {} };

            std::function<void()> copied;
            ticket result{ m_impl->add_listener(callback, copied) };

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

        operator cancellation()
        {
            return { m_impl };
        }

        void unsafe_cancel()
        {
            m_impl->unsafe_cancel();
        }

        void cancel()
        {
            std::promise<void> promise{};
            auto future = promise.get_future();
            auto ticket = add_listener([&promise]() {
                promise.set_value();
            });
            unsafe_cancel();
            future.get();
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
