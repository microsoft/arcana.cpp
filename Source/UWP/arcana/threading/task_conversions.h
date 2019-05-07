//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include "arcana/expected.h"
#include "arcana/hresult.h"
#include "arcana/threading/task.h"

#include <winrt/Windows.Foundation.h>
#include <functional>
#include <memory>

namespace arcana
{
    namespace detail
    {
        template <typename ErrorT, typename T, typename AsyncInfoT>
        void set_completion_source(arcana::task_completion_source<T, ErrorT>& completionSource, const AsyncInfoT& sender)
        {
            completionSource.complete(sender.GetResults());
        }

        template <typename ErrorT, typename AsyncInfoT>
        inline void set_completion_source(arcana::task_completion_source<void, ErrorT>& completionSource, const AsyncInfoT&)
        {
            completionSource.complete();
        }

        template <typename ResultT, typename AsyncInfoT, typename ErrorT>
        auto create_task(const AsyncInfoT& asyncInfo)
        {
            auto completionSource = arcana::task_completion_source<ResultT, ErrorT>{};

            asyncInfo.Completed([completionSource](const AsyncInfoT& sender, const winrt::Windows::Foundation::AsyncStatus status) mutable
            {
                if (status == winrt::Windows::Foundation::AsyncStatus::Completed)
                {
                    set_completion_source<ErrorT>(completionSource, sender);
                }
                else if (status == winrt::Windows::Foundation::AsyncStatus::Canceled)
                {
                    completionSource.complete(make_unexpected(std::make_error_code(std::errc::operation_canceled)));
                }
                else if (status == winrt::Windows::Foundation::AsyncStatus::Error)
                {
                    completionSource.complete(make_unexpected(arcana::error_code_from_hr(sender.ErrorCode())));
                }
            });

            return completionSource.as_task();
        }
    }

    template <typename ErrorT, typename T>
    auto create_task(const winrt::Windows::Foundation::IAsyncOperation<T>& asyncOperation)
    {
        return detail::create_task<T, winrt::Windows::Foundation::IAsyncOperation<T>, ErrorT>(asyncOperation);
    }

    template <typename ErrorT>
    inline auto create_task(const winrt::Windows::Foundation::IAsyncAction& asyncAction)
    {
        return detail::create_task<void, winrt::Windows::Foundation::IAsyncAction, ErrorT>(asyncAction);
    }

    template <typename ErrorT, typename T, typename TProgress>
    auto create_task(const winrt::Windows::Foundation::IAsyncOperationWithProgress<T, TProgress>& asyncOperation)
    {
        return detail::create_task<T, winrt::Windows::Foundation::IAsyncOperationWithProgress<T, TProgress>, ErrorT>(asyncOperation);
    }

    template <typename ErrorT, typename TProgress>
    inline auto create_task(const winrt::Windows::Foundation::IAsyncActionWithProgress<TProgress>& asyncAction)
    {
        return detail::create_task<void, winrt::Windows::Foundation::IAsyncActionWithProgress<TProgress>, ErrorT>(asyncAction);
    }
}
