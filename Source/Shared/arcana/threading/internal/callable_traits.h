#pragma once

#include "arcana/expected.h"

namespace arcana { namespace internal {

    //
    // Helper functions to determine whether or not a lambda handles arcana::expected<T>.
    //
    template<typename CallableT>
    std::true_type handles_expected_test(CallableT&& callable,
        decltype(std::declval<CallableT>()(make_unexpected(std::declval<std::error_code>())))* = nullptr);

    std::false_type handles_expected_test(...);

    //
    // Helper function to determine whether or not a function takes an expected<InputT, std::exception_ptr> or
    // and expected<InputT, std::error_code>.
    //
    template<typename InputT, typename CallableT>
    basic_expected<InputT, std::exception_ptr> expected_test(CallableT&& callable,
        decltype(std::declval<CallableT>()(std::declval<basic_expected<InputT, std::exception_ptr>>()))* = nullptr);

    template<typename InputT>
    basic_expected<InputT, std::error_code> expected_test(...);

    //
    // Extracts the return type of a callable as well as whether or not the invocation is noexcept.
    //
    template<typename CallableT, typename HandlesExpected, typename InputT>
    struct invoke_result
    {
        using type = decltype(std::declval<CallableT>()(std::declval<std::add_lvalue_reference_t<InputT>>()));
        using is_nothrow_invocable = std::bool_constant<noexcept(std::declval<CallableT>()(std::declval<std::add_lvalue_reference_t<InputT>>()))>;
    };

    template<typename CallableT>
    struct invoke_result<CallableT, std::false_type, void>
    {
        using type = decltype(std::declval<CallableT>()());
        using is_nothrow_invocable = std::bool_constant<noexcept(std::declval<CallableT>()())>;
    };

    //
    // Extracts the ErrorT of an expected
    //
    template<typename ExpectedT>
    struct expected_error
    {
        using type = void;
    };

    template<typename ValueT, typename ErrorT>
    struct expected_error<basic_expected<ValueT, ErrorT>>
    {
        using type = ErrorT;
    };

    //
    // A class that exposes all the type traits of a callable in order to wrap it in the task system.
    //
    template<typename CallableT, typename InputT>
    struct callable_traits
    {
        //
        // The type of the callable
        //
        using type = CallableT;

        //
        // The type Input of task<Input> if this callable were chained on a task.
        // This should be the raw unwrapped type of result of a previous task. (not an expected)
        //
        using task_input_type = InputT;

        //
        // Whether or not this callable takes an expected<InputT> as an input parameter
        //
        using handles_expected = decltype(handles_expected_test(std::declval<type>()));

        //
        // The actual type this callable takes as a parameter.
        // Either expected<InputT, ErrorT> or InputT
        //
        using input_type = std::conditional_t<
            handles_expected::value,
                decltype(expected_test<InputT>(std::declval<type>())),
                task_input_type>;

        //
        // The unmodified return type of this callable
        //
        using return_type = typename invoke_result<type, handles_expected, input_type>::type;

        //
        // Whether or not this callable is marked as noexcept
        //
        using is_nothrow_invocable = typename invoke_result<type, handles_expected, input_type>::is_nothrow_invocable;

        //
        // The error propagation mechanism this callable should use.
        // Either std::error_code or std::exception_ptr
        //
        using error_propagation_type = std::conditional_t<is_expected<return_type>::value,
            typename expected_error<return_type>::type,
            std::conditional_t<is_nothrow_invocable::value,
                std::error_code,
                std::exception_ptr>>;

        static_assert(std::is_same_v<error_propagation_type, std::exception_ptr> || is_nothrow_invocable::value,
            "When using error_code as your error propagation mechanism you need to mark your method noexcept");

        //
        // The expected<ReturnT, ErrorT> this callable should return when wrapped.
        //
        using expected_return_type = typename as_expected<return_type, error_propagation_type>::type;
    };
}}