//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <thread>
#include <arcana/functional/inplace_function.h>
#include <android/looper.h>
#include <array>
#include <memory>
#include <unistd.h>

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

    class looper_scheduler final
    {
    public:
        looper_scheduler(looper_scheduler&) = delete;

        explicit looper_scheduler(ALooper* looper)
            : m_looper{ looper }
        {
            if (pipe(m_fd) == -1)
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
            if (m_looper != nullptr)
            {
                ALooper_removeFd(m_looper, m_fd[0]);
                ALooper_release(m_looper);
                close(m_fd[0]);
                close(m_fd[1]);
            }
        }

        looper_scheduler(looper_scheduler&& other)
        {
            *this = std::move(other);
        }

        looper_scheduler& operator=(looper_scheduler&& other)
        {
            m_looper = other.m_looper;
            m_fd[0] = other.m_fd[0];
            m_fd[1] = other.m_fd[1];

            other.m_looper = nullptr;
            other.m_fd[0] = 0;
            other.m_fd[1] = 0;

            return *this;
        }

        template<typename CallableT>
        void operator()(CallableT&& callable) const
        {
            auto function_ptr = std::make_unique<stdext::inplace_function<void()>>([callable{ std::forward<CallableT>(callable) }]() { callable(); });
            auto raw_function_ptr = function_ptr.release();
            if (write(m_fd[1], &raw_function_ptr, sizeof(raw_function_ptr)) == -1)
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
        static int looper_callback(int fd, int events, void* data)
        {
            stdext::inplace_function<void()>* raw_function_ptr;
            if (read(fd, &raw_function_ptr, sizeof(raw_function_ptr)) == -1)
            {
                throw std::runtime_error{ std::strerror(errno) };
            }

            std::unique_ptr<stdext::inplace_function<void()>> function_ptr{ raw_function_ptr };
            (*function_ptr)();

            return 1;
        }

        ALooper* m_looper;
        int m_fd[2];
    };
}
