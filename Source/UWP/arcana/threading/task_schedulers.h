//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <winrt/Windows.System.Threading.h>
#include <winrt/Windows.UI.Core.h>

#include <mutex>
#include <unordered_map>

namespace arcana
{
    // NOTE: These task schedulers are for the arcana task system.

    namespace
    {
        constexpr auto threadpool_scheduler = [](auto&& callable)
        {
            winrt::Windows::System::Threading::ThreadPool::RunAsync(
                [callable = std::forward<decltype(callable)>(callable)](winrt::Windows::Foundation::IAsyncAction)
                {
                    callable();
                });
        };
    }

    class xaml_scheduler final
    {
    public:
        explicit xaml_scheduler(winrt::Windows::UI::Core::CoreDispatcher dispatcher)
            : m_dispatcher{ std::move(dispatcher) }
        {
        }

        template<typename CallableT>
        void operator()(CallableT&& callable) const
        {
            m_dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [callable{ std::forward<CallableT>(callable) }]{ callable(); });
        }

        static const xaml_scheduler& get_for_current_window()
        {
            const std::lock_guard<std::mutex> guard{ s_mutex };

            if (auto result = s_schedulers.find(std::this_thread::get_id()); result != s_schedulers.end())
            {
                return *result->second.get();
            }

            const auto currentWindow = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread();
            if (currentWindow == nullptr)
            {
                throw std::runtime_error("No CoreWindow associated with the current thread");
            }

            return *s_schedulers.emplace(std::this_thread::get_id(), std::make_unique<xaml_scheduler>(currentWindow.Dispatcher())).first->second.get();
        }

    private:
        using scheduler_map = std::unordered_map<std::thread::id, std::unique_ptr<xaml_scheduler>>;

        winrt::Windows::UI::Core::CoreDispatcher m_dispatcher;
        static scheduler_map s_schedulers;
        static std::mutex s_mutex;
    };
}
