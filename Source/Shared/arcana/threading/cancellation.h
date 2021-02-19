#pragma once

#include "arcana/containers/ticketed_collection.h"

#include <algorithm>
#include <cassert>
#include <atomic>
#include <memory>
#include <vector>

#include <functional>
#include <vector>

namespace arcana
{
    class cancellation
    {
        using collection = ticketed_collection<std::function<void>>;

    public:
        using ticket = collection::ticket;
        using ticket_scope = collection::ticket_scope;

        using ptr = std::shared_ptr<cancellation>;

        virtual bool cancelled() const = 0;

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
            ticket result{ internal_add_listener(callback, copied) };

            if (copied)
                copied();

            return result;
        }

        static cancellation& none();

    protected:
        cancellation() = default;
        cancellation& operator=(const cancellation&) = delete;

        virtual ~cancellation()
        {
            assert(m_listeners.empty() && "you're destroying the listener collection and you still have listeners");
        }

        template<typename CallableT>
        ticket internal_add_listener(CallableT&& callback, std::function<void()>& copied)
        {
            std::lock_guard<std::mutex> guard{ m_mutex };

            if (m_signaled)
            {
                copied = std::forward<CallableT>(callback);
                return m_listeners.insert(copied, m_mutex);
            }
            else
            {
                return m_listeners.insert(std::forward<CallableT>(callback), m_mutex);
            }
        }

        void signal_cancelled()
        {
            std::vector<std::function<void()>> listeners;

            {
                std::lock_guard<std::mutex> guard{ m_mutex };

                listeners.reserve(m_listeners.size());
                std::copy(m_listeners.begin(), m_listeners.end(), std::back_inserter(listeners));

                m_signaled = true;
            }

            // We want to signal cancellation in reverse order
            // so that if a parent function adds a listener
            // then a child function does the same, the child
            // cancellation runs first. This avoids ownership
            // semantic issues.
            for(auto itr = listeners.rbegin(); itr != listeners.rend(); ++itr)
                (*itr)();
        }

    private:
        mutable std::mutex m_mutex;
        ticketed_collection<std::function<void()>> m_listeners;
        bool m_signaled = false;
    };

    class cancellation_source : public cancellation
    {
    public:
        using ptr = std::shared_ptr<cancellation_source>;

        virtual bool cancelled() const override
        {
            return m_cancellationRequested;
        }

        void cancel()
        {
            if (m_cancellationRequested.exchange(true) == false)
            {
                signal_cancelled();
            }
        }

    private:
        std::atomic<bool> m_cancellationRequested{ false };
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
