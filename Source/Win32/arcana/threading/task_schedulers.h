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
                auto callback = [](PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work)
                {
                    auto callable_ptr = std::unique_ptr<CallableT>(static_cast<CallableT*>(context));
                    (*callable_ptr)();
                    CloseThreadpoolWork(work);
                };

                auto callable_ptr = std::make_unique<CallableT>(callable);
                PTP_WORK work = CreateThreadpoolWork(callback, callable_ptr.release(), nullptr);
                if (work == nullptr)
                {
                    throw win32_exception{ ::GetLastError() };
                }

                SubmitThreadpoolWork(work);
            }
        } threadpool_scheduler{};
    }
}
