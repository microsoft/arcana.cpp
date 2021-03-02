cmake_minimum_required(VERSION 3.10.2)

project(arcana.cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Top-level folder
set(SOURCES
    "Source/Shared/arcana/algorithm.h"
    "Source/Shared/arcana/expected.h"
    "Source/Shared/arcana/finally_scope.h"
    "Source/Shared/arcana/iterators.h"
    "Source/Shared/arcana/macros.h"
    "Source/Shared/arcana/math.h"
    "Source/Shared/arcana/sentry.h"
    "Source/Shared/arcana/string.h"
    "Source/Shared/arcana/type_traits.h")

# containers
set(SOURCES ${SOURCES}
    "Source/Shared/arcana/containers/sorted_vector.h"
    "Source/Shared/arcana/containers/ticketed_collection.h"
    "Source/Shared/arcana/containers/unique_vector.h"
    "Source/Shared/arcana/containers/unordered_bimap.h"
    "Source/Shared/arcana/containers/weak_table.h")

# experimental
set(SOURCES ${SOURCES}
    "Source/Shared/arcana/experimental/array.h")

# functional
set(SOURCES ${SOURCES}
    "Source/Shared/arcana/functional/inplace_function.h")

# messaging
set(SOURCES ${SOURCES}
    "Source/Shared/arcana/messaging/mediator.h"
    "Source/Shared/arcana/messaging/router.h")

# scheduling
set(SOURCES ${SOURCES}
    "Source/Shared/arcana/scheduling/state_machine.h"
    "Source/Shared/arcana/scheduling/state_machine_state.h")

# threading
set(SOURCES ${SOURCES}
    "Source/Shared/arcana/threading/affinity.h"
    "Source/Shared/arcana/threading/blocking_concurrent_queue.h"
    "Source/Shared/arcana/threading/cancellation.h"
    "Source/Shared/arcana/threading/coroutine.h"
    "Source/Shared/arcana/threading/dispatcher.h"
    "Source/Shared/arcana/threading/pending_task_scope.h"
    "Source/Shared/arcana/threading/task.h")

# threading/internal
set(SOURCES ${SOURCES}
    "Source/Shared/arcana/threading/internal/callable_traits.h"
    "Source/Shared/arcana/threading/internal/internal_task.h")

if(WIN32)
    set(SOURCES ${SOURCES}
        "Source/Windows/arcana/hresult.h"
        "Source/Windows/arcana/hresult.cpp"
        "Source/Windows/arcana/threading/set_thread_name.h"
        "Source/Windows/arcana/threading/set_thread_name.cpp"
        "Source/Windows/arcana/threading/task_conversions.h"
        "Source/Windows/arcana/threading/task_schedulers.h"
        "Source/Windows/arcana/threading/task_schedulers.cpp")

    add_library(arcana ${SOURCES})

    target_include_directories(arcana PUBLIC "Source/Submodules/GSL/include")
    target_include_directories(arcana PUBLIC "Source/Shared")
    target_include_directories(arcana PUBLIC "Source/Windows")

    set_target_properties(arcana PROPERTIES LINKER_LANGUAGE CXX)

    target_compile_definitions(arcana PRIVATE NOMINMAX)
    target_compile_definitions(arcana PRIVATE _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
else()
    add_library(arcana INTERFACE)

    target_include_directories(arcana INTERFACE "Source/Submodules/GSL/include")
    target_include_directories(arcana INTERFACE "Source/Shared")

    if(ANDROID)
        target_include_directories(arcana INTERFACE "Source/Android")
    elseif(APPLE)
        target_include_directories(arcana INTERFACE "Source/Apple")
    elseif(UNIX)
        target_include_directories(arcana INTERFACE "Source/Unix")
    endif()
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
