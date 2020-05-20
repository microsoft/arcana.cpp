//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <android/looper.h>
#include <arcana/functional/inplace_function.h>
#include <memory>
#include <thread>
#include <unistd.h>
#include <array>

namespace arcana
{
    // NOTE: These task schedulers are for the arcana task system.

    namespace
    {
        // TOOD: this is a stop gap for platforms that don't have a threadpool implementation
        constexpr struct
        {
            template<typename CallableT>
            void operator()(CallableT&& callable) const
            {
                std::thread([callable{ std::forward<CallableT>(callable) }]() { callable(); }).detach();
            }
        } threadpool_scheduler{};
    }

    template<size_t WorkSize>
    class looper_scheduler final
    {
    public:
        looper_scheduler(const looper_scheduler&) = delete;
        looper_scheduler& operator=(const looper_scheduler&) = delete;

        explicit looper_scheduler(ALooper* looper)
            : m_looper{ looper }
        {
            if (pipe(m_fd.data()) == -1)
            {
                throw std::runtime_error{ std::strerror(errno) };
            }

            ALooper_acquire(m_looper);

            if (ALooper_addFd(m_looper, m_fd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, looper_callback, nullptr) == -1)
            {
                throw std::runtime_error{ "Failed to add file descriptor to looper" };
            }
        }

        ~looper_scheduler()
        {
            destroy();
        }

        looper_scheduler(looper_scheduler&& other)
            : m_looper{ other.m_looper }
            , m_fd{ other.m_fd }
        {
            other.m_looper = nullptr;
            other.m_fd = { -1, -1 };
        }

        looper_scheduler& operator=(looper_scheduler&& other)
        {
            if (this != &other)
            {
                destroy();
                m_looper = other.m_looper;
                m_fd = other.m_fd;
                other.m_looper = nullptr;
                other.m_fd = { -1, -1 };
            }

            return *this;
        }

        template<typename CallableT>
        void operator()(CallableT&& callable) const
        {
            auto callback_ptr = std::make_unique<callback_t>([callable{ std::forward<CallableT>(callable) }]() { callable(); });
            auto raw_callback_ptr = callback_ptr.release();
            if (write(m_fd[1], &raw_callback_ptr, sizeof(raw_callback_ptr)) == -1)
            {
                throw std::runtime_error{ std::strerror(errno) };
            }
        }

        static looper_scheduler get_for_current_thread()
        {
            ALooper* looper = ALooper_forThread();
            if (looper == nullptr)
            {
                throw std::runtime_error("No looper associated with the current thread");
            }

            return looper_scheduler{ looper };
        }

    private:
        using callback_t = stdext::inplace_function<void(), WorkSize>;

        void destroy()
        {
            if (m_looper != nullptr)
            {
                ALooper_removeFd(m_looper, m_fd[0]);
                ALooper_release(m_looper);
                close(m_fd[0]);
                close(m_fd[1]);
            }
        }

        static int looper_callback(int fd, int /*events*/, void* /*data*/)
        {
            callback_t* raw_callback_ptr;
            if (read(fd, &raw_callback_ptr, sizeof(raw_callback_ptr)) == -1)
            {
                throw std::runtime_error{ std::strerror(errno) };
            }

            std::unique_ptr<callback_t> callback_ptr{ raw_callback_ptr };
            (*callback_ptr)();

            return 1;
        }

        ALooper* m_looper;
        std::array<int, 2> m_fd;
    };
}
