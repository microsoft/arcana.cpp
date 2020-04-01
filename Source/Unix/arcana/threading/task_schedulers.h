//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

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
}
