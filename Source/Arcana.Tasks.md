# Arcana Task System Overview

The Arcana Task system provides a low overhead, flexible, cross-platform C++ implementation of task-based asynchrony. It supports cancellation, multiple continuations, custom schedulers, coroutines, and error propagation via std::error_code or std::exception_ptr.

## Examples

- [Starting a Task](#starting-a-task)
- [Task Continuations](#task-continuations)
- [Returning a Task from a Coroutine](#returning-a-task-from-a-coroutine)
- [Awaiting a Task in a Coroutine](#awaiting-a-task-in-a-coroutine)

## Reference

- [Package](#package)
- [Includes](#includes)
- [Error Types](#error-types)
- [Schedulers](#schedulers)
- [Cancellation](#cancellation)
- [Continuations](#continuations)
- [Coroutines](#coroutines)

# Examples

## Starting a Task

To start a task, `#include <arcana/threading/task.h>` and use `arcana::make_task`

```c++
arcana::background_dispatcher<32> scheduler;
auto task = arcana::make_task(scheduler, arcana::cancellation::none(), []
{
    // Doing work on a background thread.
});
```

In this example, a [`background_dispatcher`](#background_dispatcher) is used to schedule the work, and there is no [cancellation](#cancellation) signal involved.

## Task Continuations

To add a continuation to an existing task, use `arcana::task<ResultT, ErrorT>::then`

```c++
auto antecedentTask = arcana::task_from_result<std::exception_ptr, int>(42);
auto continuationTask = antecedentTask.then(arcana::inline_scheduler, arcana::cancellation::none(), [](int result)
{
    return result * result;
});
```

In this example, the [`arcana::inline_scheduler`](#inline_scheduler) is used to schedule the continuation synchronously, and again there is no [cancellation](#cancellation) signal involved.

## Returning a Task from a Coroutine

To create a coroutine that returns an Arcana Task, `#include <arcana/threading/coroutine.h>` and implement a function that returns an `arcana::task<ResultT, ErrorT>` and contains a co_await and/or a co_return.

```c++
arcana::task<int, std::exception_ptr> DoSomethingAsync()
{
    co_return 42;
}
```

In this example, for simplicity the coroutine is synchronous.

## Awaiting a Task in a Coroutine

To await an Arcana Task within a coroutine, `#include <arcana/threading/coroutine.h>`, and co_await a `arcana::task<ResultT, ErrorT>` wrapped in a call to `arcana::configure_await`

```c++
arcana::task<int, std::exception_ptr> DoSomethingElseAsync()
{
    int result = co_await arcana::configure_await(arcana::inline_scheduler, DoSomethingAsync());
    return result * result;
}
```

Note that Arcana Tasks are not directly awaitable, because a given Arcana Task does not have the context to make an intelligent decision on how to schedule the resumption of the coroutine. Some task systems attempt to provide reasonable defaults, but this usually leads to confusion and bugs, so when using the Arcana Task system, a call to `arcana::configure_await` is always required to specify the scheduler on which to resume the coroutine.

In this example, the [`arcana::inline_scheduler`](#inline_scheduler) is used for simplicity, but in practice this should be done with caution.

# Reference

## Includes

| Header                                | Description                                       |
| ------------------------------------- | ------------------------------------------------- |
| arcana/threading/task.h               | The core Arcana Task system.                      |
| arcana/threading/coroutine.h          | Coroutine support for the Arcana Task system.     |
| arcana/threading/task_schedulers.h    | Additional schedulers for the Arcana Task system. |

## Error Types

`arcana::task<ResultT, ErrorT>` is templated on both the type of the result and the type of the error. The Arcana Task system currently supports two types of errors: std::exception_ptr and std::error_code.

By default, `arcana::make_task` and `arcana::task<ResultT, ErrorT>::then` return an `arcana::task<ResultT, std::exception_ptr>`. To create an `arcana::task<ResultT, std::error_code>`, just specify a `noexcept` callable.

```c++
arcana::background_dispatcher<32> scheduler;
auto task = arcana::make_task(scheduler, arcana::cancellation::none(), []() noexcept
{
    // Doing work on a background thread.
});
```

Using std::error_code instead of std::exception_ptr eliminates exception overhead, which is useful in highly performance sensitive code.

When returning `arcana::task<void, std::error_code>` from a coroutine, note that due to limitations of the C++ coroutine system, the coroutine must always return a value (not void). In the case of success, return `arcana::coroutine_success`.

```c++
arcana::task<void, std::error_code> DoSomethingElseAsync()
{
    co_return arcana::coroutine_success;
}
```

## Schedulers

When a task is started (`arcana::make_task`) or a task continuation is registered (`arcana::task<ResultT, ErrorT>::then`), a scheduler is specified to determine the context in which the task body is executed. A scheduler is simply a callable that accepts a parameterless callable as its argument, and invokes the passed in parameterless callable in some context at some point in time (e.g. schedules the work). As such, this mechanism is completely customizable/extensible. The Arcana Task system includes several default schedulers.

**Dispatchers** are a specific type of scheduler that have a work queue and in some way process the work in that queue. Some of the default schedulers are dispatcher based (derive from the `arcana::dispatcher` base class). Dispatchers are most commonly used to schedule tasks, but they can be used outside the context of the task system as well.

### inline_scheduler

`arcana::inline_scheduler` executes work synchronously. When using `arcana::inline_scheduler` with `arcana::task<ResultT, ErrorT>::then`, there are two possibilities:

1. The antecedent task is already in a completed state, in which case the continuation runs synchronously when `arcana::task<ResultT, ErrorT>::then` is invoked.

2. The antecedent task is not already in a completed state, in which case the continuation runs synchronously when the antecedent task completes, in the context of whatever scheduler ran the antecedent task.

### manual_dispatcher

`manual_dispatcher` is a dispatcher that is manually/externally *ticked*. This is useful when adapting the Arcana Task system to another system with an existing execution context, such as a render/UI thread. Invoke the `manual_dispatcher::tick` function to drain the current work queue.

```c++
arcana::manual_dispatcher<32> scheduler;
auto task = arcana::make_task(scheduler, arcana::cancellation::none(), []
{
    // Doing work on a background thread.
});

// Manually "tick" the dispatcher to process the work queue.
// In this example, the task would be executed synchronously since tick is called synchronously.
scheduler.tick(arcana::cancellation::none());
```

### background_dispatcher

`background_dispatcher` is a dispatcher that creates and owns a thread and executes work on that thread as aggressively as possible. The thread waits in a blocked state when no work is queued.

```c++
arcana::background_dispatcher<32> scheduler;
auto task = arcana::make_task(scheduler, arcana::cancellation::none(), []
{
    // Doing work on a background thread.
});
```

### threadpool_scheduler

`threadpool_scheduler` is a scheduler that uses a threadpool to schedule work.

* UWP uses the Windows Runtime [`ThreadPool`](https://docs.microsoft.com/en-us/uwp/api/Windows.System.Threading.ThreadPool)
* Win32 uses the [Win32 Threadpool](https://docs.microsoft.com/en-us/windows/win32/procthread/thread-pools)
* Other platforms currently fall back to creating a separate thread using `std::thread` as a stop gap solution.

```c++
auto task = arcana::make_task(arcana::threadpool_scheduler, arcana::cancellation::none(), []
{
    // Doing work on a threadpool thread.
});
```

### xaml_scheduler

`xaml_scheduler` is a Windows specific scheduler that uses the Windows Runtime [`CoreDispatcher`](https://docs.microsoft.com/en-us/uwp/api/windows.ui.core.coredispatcher). To get an instance of the `xaml_scheduler`, invoke the `xaml_scheduler::get_for_current_window` function on a thread with an associated `CoreDispatcher` (a UI thread associated with a `Window`).

```c++
auto task = arcana::make_task(arcana::xaml_scheduler::get_for_current_window(), arcana::cancellation::none(), []
{
    // Doing work on the Windows Runtime core dispatcher.
});
```

## Cancellation

Cancellation is ultimately managed via `arcana::cancellation`. An owner of the cancellation policy should create an `arcana::cancellation_source` and pass it to consumers as an `arcana::cancellation`, as only the owner should be able to request cancellation via `arcana::cancellation_source::cancel`. `arcana::cancellation` instances are passed by reference, so it is the owner's responsibility to keep the instance alive until all associated asynchronous work is completed.

The result of a canceled `arcana::task<ResultT, std::exception_ptr>` will be an `arcana::expected<ResultT, std::exception_ptr>` in an error state containing a `std::system_error` containing a `std::error_code` containing `std::errc::operation_canceled`.

The result of a canceled `arcana::task<ResultT, std::error_code>` will be an `arcana::expected<ResultT, std::error_code>` in an error state containing a `std::error_code` containing `std::errc::operation_canceled`.

When calling a function that expects an `arcana::cancellation` but the intent is for the operation to not be cancellable, pass in `arcana::cancellation::none()`.

### Cancellation at the Task Boundary

`arcana::make_task` and `arcana::task<ResultT, ErrorT>::then` both accept an `arcana::cancellation`. If the `arcana::cancellation` enters a canceled state before the task begins, the task will itself complete in a canceled state (via a `std::error_code` with `std::errc::operation_canceled`). Once a task begins, a request to cancel (via `arcana::cancellation_source::cancel`) will not affect the task.

```c++
arcana::cancellation_source m_source;

...

arcana::task<void, std::exception_ptr> DoSomethingAsync(arcana::cancellation& token)
{
    return arcana::make_task(arcana::threadpool_scheduler, token, []
    {
        // Do something on a threadpool thread
    }).then(arcana::threadpool_scheduler, token, []
    {
        // Do another thing on a threadpool thread
    });
}

...

DoSomethingAsync(m_source);
m_source.cancel();
```

In this example, the first task would be synchronously queued in the threadpool and run at a later time, but the second task would likely be canceled before even being queued in the thread pool.

### Cancellation within a Task Body

For fine grained cancellation, the task body (the callable) should directly use the `arcana::cancellation` instance (as a member variable, captured variable as part of a lambda closure, coroutine stack etc.). In the case of `arcana::task<ResultT, std::exception_ptr>`, `arcana::cancellation::throw_if_cancellation_requested()` can be used.

```c++
auto task = arcana::make_task(arcana::threadpool_scheduler, cancellation, [&cancellation]
{
    // Do some work...
    cancellation.throw_if_cancellation_requested();
    // Maybe do some more work...
});
```

In the case of `arcana::task<ResultT, std::error_code>`, it would be more typical to check the state of the `arcana::cancellation` and then potentially return an error code.

```c++
auto task = arcana::make_task(arcana::threadpool_scheduler, cancellation, [&cancellation]() noexcept
{
    // Do some work...
    if (cancellation.cancelled())
    {
        return arcana::unexpected{ std::errc::operation_canceled };
    }
    // Maybe do some more work...
});
```

In the case of coroutines, cancellation within the task body is the only option (cancellation does not occur on task boundaries).

```c++
arcana::task<void, std::exception_ptr> DoSomethingAsync(arcana::cancellation& cancellation)
{
    // Do some synchronous work

    // Await some async work
    co_await arcana::configure_await(arcana::inline_scheduler, DoSomethingElseAsync(cancellation));

    // Maybe do some more work
    cancellation.throw_if_cancellation_requested();
}
```

## Continuations

When registering task continuations with `arcana::task<ResultT, ErrorT>::then`, there are two options for the input parameters of the callable:

**`ResultT`** - when the continuation should only be invoked if then antecedent task was successful. If the antecedent task completed in an error state, the underlying error will automatically be propagated to the task returned from `arcana::task<ResultT, ErrorT>::then`, and the task body will not execute.

```c++
arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), []
{
    throw std::logic_error("Something went wrong!");
    return 42;
}).then(arcana::inline_scheduler, arcana::cancellation::none(), [](int result)
{
    // This callable will not be invoked, but the arcana::task<void, std::exception_ptr> returned from this call will contain the exception from the antecedent task.
});
```

**`arcana::expected<ResultT, ErrorT>`** - when the continuation should be invoked even if the antecedent task completes in an error state. The task body will execute (unless the task was canceled before it started), and it is up to the continuation(s) to decide what to do with the error.

```c++
arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), []
{
    throw std::logic_error("Something went wrong!");
    return 42;
}).then(arcana::inline_scheduler, arcana::cancellation::none(), [](const arcana::expected<int, std::exception_ptr>& result)
{
    // This callable will be invoked, and should make an explicit decision on how to handle the case where the antecedent task completes in an error state.
    if (result.has_error())
    {
        try
        {
            std::rethrow_exception(result.error());
        }
        catch (const std::logic_error&)
        {
            // Handle std::logic_error
        }
    }
});
```

## Coroutines

When returning an `arcana::task<ResultT, std::exception_ptr>` from a coroutine, the coroutine body should either return a ResultT or throw an exception. When awaiting an `arcana::task<ResultT, std::exception_ptr>` (via `arcana::configure_await`), wrap the call in a try/catch if you want to handle exceptions.

When returning an `arcana::task<ResultT, std::error_code>` from a coroutine, the coroutine body should return a Result to or return a std::error_code (when ResultT is void, the coroutine should return `arcana::coroutine_success` when not returning a real error). When awaiting an `arcana::task<ResultT, std::error_code>`, there are two options. The first is to assign the result to an `arcana::expected<ResultT, std::error_code>`. This enables observing and reacting to failure (similar to a try/catch with `arcana::task<ResultT, std::exception_ptr>`).

```c++
arcana::expected<void, std::error_code> result = co_await arcana::configure_await(arcana::inline_scheduler, DoSomethingAsync());
if (result.has_error())
{
    if (result.error() == std::errc::invalid_argument)
    {
        // Handle std::errc::invalid_argument
    }
}
```

The second option is to assign the result to ResultT (or ignore the result). In this case, if the awaited task completes in an error state, the coroutine stops executing and the error is propagated in one of the following ways:

- If an `arcana::task<ResultT, std::error_code>` is being returned, the `std::error_code` from the awaited task is propagated to the returned task.
- Otherwise, a `std::system_error` containing the `std::error_code` is thrown from the coroutine (which will automatically be propagated to a returned `arcana::task<ResultT, std::exception_ptr>`, `std::future<ResultT>`, etc.).

### switch_to

In the context of coroutines, it is possible to use `arcana::switch_to` to transition to a different execution context (via a scheduler) without running any tasks.

```c++
arcana::task<void, std::exception_ptr> DoSomethingAsync()
{
    // Assuming this function is called on a thread with an associated CoreDispatcher, store a reference to the associated xaml_scheduler.
    auto xaml_scheduler = arcana::xaml_scheduler::get_for_current_window();
    // Do some work on the UI thread

    // Switch to a threadpool thread
    co_await arcana::switch_to(arcana::threadpool_scheduler);
    // Do some work on a threadpool thread

    // Switch back to the UI thread
    co_await arcana::switch_to(xaml_scheduler);
    // Do some work on the UI thread
}
```