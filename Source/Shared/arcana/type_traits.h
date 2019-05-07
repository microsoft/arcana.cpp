#pragma once

#include <string>
#include <tuple>
#include <type_traits>

namespace arcana
{
    /*
        returns whether or not a set of variadic template arguments
        start with another set of variadic template arguments
    */
    template<typename Left, typename Right>
    struct starts_with : std::false_type
    {
    };

    template<typename Same, typename... Left, typename... Right>
    struct starts_with<std::tuple<Same, Left...>, std::tuple<Same, Right...>>
        : starts_with<std::tuple<Left...>, std::tuple<Right...>>
    {
    };

    template<typename... Right>
    struct starts_with<std::tuple<>, std::tuple<Right...>> : std::true_type
    {
    };

    namespace traits
    {
        template<typename CharT, typename Traits, typename Allocator>
        std::true_type is_string_test(const std::basic_string<CharT, Traits, Allocator>&);
        std::false_type is_string_test(...);
    }

    template<typename T>
    constexpr bool is_string()
    {
        return std::is_same<std::decay_t<decltype(traits::is_string_test(std::declval<T>()))>, std::true_type>::value;
    }

    template<typename T>
    struct static_assert_value
    {
        static constexpr bool value = T::value;

        static_assert(T::value, "assertion failed: see below for more details");
    };

    // converts an enum value to it's underlying integral representation
    template<typename T>
    constexpr auto underlying_cast(const T elem)
    {
        static_assert(std::is_enum<T>::value, "underlying_cast only operates on enum types");

        return static_cast<std::underlying_type_t<T>>(elem);
    }

    template<typename T>
    constexpr const auto& underlying_ref_cast(const T& elem)
    {
        static_assert(std::is_enum<T>::value, "underlying_cast only operates on enum types");

        return reinterpret_cast<const std::underlying_type_t<T>&>(elem);
    }

    namespace internal
    {
        template<typename CallableT, typename = decltype(std::declval<CallableT>()())>
        std::true_type void_callable(const CallableT&);
        std::false_type void_callable(...);

        template<typename CallableT, typename ArgT>
        auto invoke_with_optional_parameter(CallableT&& callable, ArgT&& arg, std::false_type)
        {
            return callable(arg);
        }

        template<typename CallableT, typename ArgT>
        auto invoke_with_optional_parameter(CallableT&& callable, ArgT&& /*arg*/, std::true_type)
        {
            return callable();
        }
    }

    template<typename T>
    constexpr auto& underlying_ref_cast(T& elem)
    {
        static_assert(std::is_enum<T>::value, "underlying_cast only operates on enum types");

        return reinterpret_cast<std::underlying_type_t<T>&>(elem);
    }

    template<typename CallableT, typename ArgT>
    auto invoke_with_optional_parameter(CallableT&& callable, ArgT&& arg)
    {
        return internal::invoke_with_optional_parameter(
            std::forward<CallableT>(callable), std::forward<ArgT>(arg), decltype(internal::void_callable(callable)){});
    }

    // helper struct to pass type information into a function.
    // Mostly for use with constructors, because you can't pass in
    // template arguments to them.
    template<typename T>
    struct type_of
    {
        using type = T;
    };
    
    template<typename T>
    auto hash(const T& object)
    {
        return std::hash<T>{}(object);
    }

    template<typename ...BoolConstantTs>
    struct count_true;

    template<typename FirstBoolConstant, typename ...BoolConstantTs>
    struct count_true<FirstBoolConstant, BoolConstantTs...> : std::integral_constant<size_t,
        (FirstBoolConstant::value ? 1 : 0) + count_true<BoolConstantTs...>::value>
    {};

    template<>
    struct count_true<> : std::integral_constant<size_t, 0>
    {};

    namespace internal
    {
        template<bool found, size_t Idx, typename ...FlagTs>
        struct find_first_index_internal;

        template<size_t Idx, typename ...FlagTs>
        struct find_first_index_internal<true, Idx, FlagTs...> : std::integral_constant<size_t, Idx - 1>
        {}; // remove one to counter-act the fact that we start at 1 in order to support the empty set case

        template<size_t Idx, typename FirstFlagT, typename ...FlagTs>
        struct find_first_index_internal<false, Idx, FirstFlagT, FlagTs...> : std::integral_constant<size_t,
            find_first_index_internal<FirstFlagT::value, Idx + 1, FlagTs...>::value>
        {};

        template<size_t Idx>
        struct find_first_index_internal<false, Idx> : std::integral_constant<size_t, Idx>
        {};
    }

    template<typename ...FlagTs>
    using find_first_index = internal::find_first_index_internal<false, 0, FlagTs...>;

    struct void_placeholder {};

    template<typename T>
    struct void_passthrough
    {
        using type = T;
    };

    template<>
    struct void_passthrough<void>
    {
        using type = void_placeholder;
    };

    namespace internal
    {
        template<typename Left, typename Right, bool Less>
        struct largest_integral_constant_impl
        {
            using type = Left;
        };

        template<typename Left, typename Right>
        struct largest_integral_constant_impl<Left, Right, true>
        {
            using type = Right;
        };
    }

    template<typename Left, typename Right>
    struct largest_integral_constant : public internal::largest_integral_constant_impl<Left, Right, Left::value < Right::value>
    {};

    template <typename T>
    struct function_traits; // Most types are not pointers to member methods. Don't implement this.

    template <typename Return, typename... Args>
    struct function_traits<Return(Args...)>
    {
        template <std::size_t i>
        struct argument
        {
            using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
        };

        template <std::size_t i>
        using argument_t = typename argument<i>::type;

        [[nodiscard]]
        static constexpr std::size_t argument_count() noexcept
        {
            return std::index_sequence_for<Args...>::size();
        }

        using returns = Return;
        using void_return = std::is_same<Return, void>;
    };

    // See https://stackoverflow.com/questions/28105077/how-can-i-get-the-class-of-a-member-function-pointer for how to get class from pointer to member function.
    template <typename T>
    struct member_function_traits; // Most types are not pointers to member methods. Don't implement this.

    template <typename Class, typename Return, typename... Args>
    struct member_function_traits<Return(Class::*)(Args...)> : public function_traits<Return(Args...)>
    {
        using type = Class;
    };

    template <typename Class, typename Return, typename... Args>
    struct member_function_traits<Return(Class::*)(Args...) const> : public function_traits<Return(Args...)>
    {
        using type = Class;
    };
}
