//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "task_schedulers.h"

namespace arcana
{
    xaml_scheduler::scheduler_map xaml_scheduler::s_schedulers = {};
    std::mutex xaml_scheduler::s_mutex = {};
}
