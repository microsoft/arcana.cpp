#pragma once

#include "type_traits.h"

#include <gsl/gsl>

#include <system_error>
#include <variant>

namespace arcana
{
    template<typename Type>
    struct is_expected;

    template<typename T, typename E>
    class basic_expected;

    namespace internal
    {
        template<typename T>
        struct expected_error_traits;

        template<>
        struct expected_error_traits<std::error_code>
        {
            template<typename ErrorT>
            using is_convertible = std::bool_constant<
                std::is_error_code_enum<std::decay_t<ErrorT>>::value ||
                std::is_error_condition_enum<std::decay_t<ErrorT>>::value
            >;

            template<typename ErrorT>
            using enable_conversion = std::enable_if<
                is_convertible<ErrorT>::value
            >;

            template<typename ErrorT>
            static std::error_code convert(ErrorT errorEnum)
            {
                using std::make_error_code;

                return make_error_code(errorEnum);
            }
        };

        template<>
        struct expected_error_traits<std::exception_ptr>
        {
            template<typename ErrorT>
            using enable_conversion = std::enable_if<
                std::is_same<std::error_code, ErrorT>::value || // support conversion from error_codes
                expected_error_traits<std::error_code>::template is_convertible<ErrorT>::value // and also enumerations that are error codes
            >;

            static std::exception_ptr convert(std::error_code code)
            {
                return std::make_exception_ptr(std::system_error(code));
            }

            template<typename ErrorT, typename = typename expected_error_traits<std::error_code>::template enable_conversion<ErrorT>::type>
            static std::exception_ptr convert(ErrorT errorEnum)
            {
                return convert(
                    expected_error_traits<std::error_code>::convert(errorEnum));
            }
        };
    }

    class bad_expected_access : public std::exception
    {
    public:
        const char* what() const noexcept override
        {
            return "tried accessing value()/error() of an expected when it wasn't set";
        }
    };

    template<typename E>
    class unexpected
    {
    public:
        unexpected() = delete;

        constexpr explicit unexpected(const E& e)
            : m_e{ e }
        {}

        constexpr explicit unexpected(E&& e)
            : m_e{ std::move(e) }
        {}

        constexpr const E& value() const &
        {
            return m_e;
        }

        constexpr E& value() &
        {
            return m_e;
        }

        constexpr E&& value() &&
        {
            return std::move(m_e);
        }
    private:
        E m_e;
    };

    template<typename T>
    inline auto make_unexpected(T&& error)
    {
        return unexpected<std::decay_t<T>>{std::forward<T>(error)};
    }

    template<typename T, typename E>
    class basic_expected
    {
        using traits = internal::expected_error_traits<E>;

        template<typename E2,
            typename = typename traits::template enable_conversion<E2>::type>
        std::variant<E, T> convert_variant(const basic_expected<T, E2>& exp)
        {
            if (exp.has_error())
                return { traits::convert(exp.error()) };
            else
                return { exp.value() };
        }
    public:
        using value_type = T;
        using error_type = E;

        basic_expected(const value_type& value)
            : m_data{ value }
        {}

        basic_expected(value_type&& value)
            : m_data{ std::move(value) }
        {}
        
        template<typename ErrorT,
            typename = typename traits::template enable_conversion<ErrorT>::type>
        basic_expected(const unexpected<ErrorT>& errorEnum)
            : m_data{ traits::convert(errorEnum.value()) }
        {}

        basic_expected(const unexpected<error_type>& ec)
            : m_data{ ec.value() }
        {
            GSL_CONTRACT_CHECK("you should never build an basic_expected<T,E> with a non-error", static_cast<bool>(error()));
        }

        basic_expected(unexpected<error_type>&& ec)
            : m_data{ std::move(ec).value() }
        {
            GSL_CONTRACT_CHECK("you should never build an basic_expected<T,E> with a non-error", static_cast<bool>(error()));
        }

        template<typename ErrorT,
            typename = typename traits::template enable_conversion<ErrorT>::type>
        basic_expected(const basic_expected<value_type, ErrorT>& other)
            : m_data{ convert_variant(other) }
        {}

        template<typename ErrorT,
            typename = typename traits::template enable_conversion<ErrorT>::type>
        basic_expected& operator=(const basic_expected<value_type, ErrorT>& other)
        {
            m_data = convert_variant(other);
        }

        //
        // Returns the expected value or throws bad_expected_access if it is in an error state.
        //
        const value_type& value() const
        {
            return const_cast<basic_expected*>(this)->value();
        }

        value_type& value()
        {
            auto val = std::get_if<value_type>(&m_data);
            if (val == nullptr)
            {
                throw bad_expected_access();
            }
            return *val;
        }

        //
        // Returns the value contained or the default passed in if the expected is in an error state.
        //
        template<typename U>
        value_type value_or(U&& def) const
        {
            if (!has_value())
            {
                return std::forward<U>(def);
            }

            return value();
        }

        //
        // Returns the error or throws bad_expected_access if it is not in an error state.
        //
        const error_type& error() const
        {
            auto err = std::get_if<error_type>(&m_data);
            if (err == nullptr)
            {
                throw bad_expected_access();
            }
            return *err;
        }

        //
        // Returns whether or not the expected contains a value.
        //
        bool has_value() const noexcept
        {
            return std::holds_alternative<value_type>(m_data);
        }

        //
        // Returns whether or not the expected is in an error state.
        //
        bool has_error() const noexcept
        {
            return std::holds_alternative<error_type>(m_data);
        }

        //
        // Converts to true if the value is valid.
        //
        explicit operator bool() const noexcept
        {
            return has_value();
        }

        //
        // Shorthand access operators
        //
        const value_type& operator*() const
        {
            return value();
        }

        value_type& operator*()
        {
            return value();
        }

        //
        // Shorthand access operators
        //
        const value_type* operator->() const
        {
            return &value();
        }

        value_type* operator->()
        {
            return &value();
        }

    private:
        std::variant<error_type, value_type> m_data;
    };

    template<typename E>
    class basic_expected<void, E>
    {
    public:
        using value_type = void;
        using error_type = E;

        using traits = internal::expected_error_traits<error_type>;

        template<typename ErrorT,
            typename = typename traits::template enable_conversion<ErrorT>::type>
        basic_expected(const unexpected<ErrorT>& errorEnum)
            : m_error{ traits::convert(errorEnum.value()) }
        {}

        basic_expected(const unexpected<error_type>& ec)
            : m_error{ ec.value() }
        {
            GSL_CONTRACT_CHECK("you should never build an basic_expected<T,E> with a non-error", static_cast<bool>(error()));
        }

        basic_expected(unexpected<error_type>&& ec)
            : m_error{ std::move(ec).value() }
        {
            GSL_CONTRACT_CHECK("you should never build an basic_expected<T,E> with a non-error", static_cast<bool>(error()));
        }

        template<typename ErrorT,
            typename = typename traits::template enable_conversion<ErrorT>::type>
        basic_expected(const basic_expected<value_type, ErrorT>& other)
            : m_error{ other.has_error() ? traits::convert(other.error()) : nullptr }
        {}

        template<typename ErrorT,
            typename = typename traits::template enable_conversion<ErrorT>::type>
        basic_expected& operator=(const basic_expected<value_type, ErrorT>& other)
        {
            m_error = other.has_error() ? traits::convert(other.error()) : nullptr;
        }

        //
        // Returns the error, throws bad_expected_access if it is not in an error state
        //
        const error_type& error() const
        {
            if (!has_error())
            {
                throw bad_expected_access();
            }
            return m_error;
        }

        //
        // Returns whether or not the expected is in an error state.
        //
        bool has_error() const noexcept
        {
            return static_cast<bool>(m_error);
        }

        //
        // Converts to true if the value is valid.
        //
        explicit operator bool() const noexcept
        {
            return !has_error();
        }

        //
        // Creates an expected<void> that doesn't have an error
        //
        static basic_expected make_valid()
        {
            return basic_expected{};
        }

    private:
        basic_expected() = default;

        error_type m_error;
    };

    template<typename Type>
    struct is_expected : public std::false_type
    {};

    template<typename Type, typename Error>
    struct is_expected<basic_expected<Type, Error>> : public std::true_type
    {};

    template<typename ResultT, typename ErrorT>
    struct as_expected
    {
        using type = basic_expected<ResultT, ErrorT>;
        using value_type = typename type::value_type;
        using error_type = typename type::error_type;
    };

    template<typename ResultT, typename ErrorT, typename DesiredErrorT>
    struct as_expected<basic_expected<ResultT, ErrorT>, DesiredErrorT>
    {
        static_assert(std::is_same<ErrorT, DesiredErrorT>::value, "the error type of the expected and the desired error type don't match");

        using type = basic_expected<ResultT, ErrorT>;
        using value_type = typename type::value_type;
        using error_type = typename type::error_type;
    };

    template<typename ResultT, typename ErrorT>
    using expected = basic_expected<ResultT, ErrorT>;

    template<typename ErrorT>
    struct error_priority;

    template<>
    struct error_priority<std::error_code> : std::integral_constant<int, 0>
    {
        using type = std::error_code;
    };

    template<>
    struct error_priority<std::exception_ptr> : std::integral_constant<int, 1>
    {
        using type = std::exception_ptr;
    };

    template<typename Left, typename Right>
    struct largest_error
    {
        using type = typename largest_integral_constant<error_priority<Left>, error_priority<Right>>::type::type;
    };

    namespace internal
    {
        template<typename Expected, typename OrType, bool IsExpected>
        struct expected_error_or_impl
        {
            using type = OrType;
        };

        template<typename Expected, typename OrType>
        struct expected_error_or_impl<Expected, OrType, true>
        {
            using type = typename Expected::error_type;
        };
    }

    template<typename Expected, typename OrType>
    struct expected_error_or : public internal::expected_error_or_impl<Expected, OrType, is_expected<Expected>::value>
    {};
}
