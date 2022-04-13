#pragma once

#include <gsl/gsl>
#include <codecvt>
#include <locale>
#include <string>

#ifdef _MSC_VER
#include <string_view>
#else
// llvm-libc++ included with Android NDK r15c hasn't yet promoted the C++1z library fundamentals
// technical specification to the final C++17 specification for string_view.
// Force promote the types we need up into the std namespace.
#include <experimental/string_view>
namespace std
{
    using string_view = experimental::string_view;
    using wstring_view = experimental::wstring_view;
}
#endif

namespace arcana
{
    inline std::string utf16_to_utf8(gsl::cwzstring input)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.to_bytes(input);
    }

    inline std::wstring utf8_to_utf16(gsl::czstring input)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(input);
    }

    inline std::string utf16_to_utf8(std::wstring_view input)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.to_bytes(input.data(), input.data() + input.size());
    }

    inline std::wstring utf8_to_utf16(std::string_view input)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(input.data(), input.data() + input.size());
    }

    struct string_compare
    {
        using is_transparent = std::true_type;

        bool operator()(std::string_view a, std::string_view b) const
        {
            return a.compare(b);
        }

        bool operator()(gsl::span<char> a, gsl::span<char> b) const
        {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
        }

        bool operator()(gsl::czstring a, gsl::czstring b) const
        {
            return strcmp(a, b) < 0;
        }
    };
}
