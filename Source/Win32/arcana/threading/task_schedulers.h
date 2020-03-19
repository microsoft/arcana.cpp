//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <Windows.h>
#include <memory>
#include <functional>
#include <arcana/win32_exception.h>

namespace arcana
{
    // NOTE: These task schedulers are for the arcana task system.

    namespace
    {
        constexpr struct
        {
            template<typename CallableT>
            void operator()(CallableT&& callable) const
            {
                auto callback = [](PVOID context) -> DWORD
                {
                    auto callable_ptr = std::unique_ptr<CallableT>(static_cast<CallableT*>(context));
                    (*callable_ptr)();
                    return 0;
                };

                auto callable_ptr = std::make_unique<CallableT>(callable);
                if (!QueueUserWorkItem(callback, callable_ptr.release(), 0))
                {
                    throw win32_exception{ GetLastError() };
                }
            }
        } threadpool_scheduler{};
    }
}
