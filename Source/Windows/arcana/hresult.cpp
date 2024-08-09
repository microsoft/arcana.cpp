//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "hresult.h"
#include "arcana/string.h"
#include "arcana/type_traits.h"
#include "arcana/containers/unordered_bimap.h"
#include <winrt/base.h>
#include <cassert>
#include <future>
#include <shared_mutex>
#include <system_error>
#include <unordered_map>
#include <optional>

namespace arcana
{
    namespace
    {
        // sets bit C(1) for customer (29th index of 32 bit int)
        // See https://msdn.microsoft.com/en-us/library/cc231198.aspx
        constexpr int32_t CustomerBitMask = 0x20000000;

        constexpr int32_t make_hresult_code(int32_t categoryFacility, int32_t code)
        {
            return MAKE_HRESULT(SEVERITY_ERROR, categoryFacility, code) | CustomerBitMask;
        }

        class category_storage
        {
        public:
            category_storage()
            {
                add_category(std::generic_category());
                add_category(std::iostream_category());
                add_category(std::future_category());
                add_category(std::system_category());
            }

            void add_category(const std::error_category& category)
            {
                const std::lock_guard<std::shared_mutex> lock{ m_mutex };
                m_storageBimap.emplace(&category, m_categoryFacility++);
            }

            std::optional<int> get_facility(const std::error_category& category) const
            {
                const std::shared_lock<std::shared_mutex> lock{ m_mutex };

                const auto iter = m_storageBimap.left().find(&category);
                if (iter != m_storageBimap.left().end())
                {
                    return iter->second;
                }
                else
                {
                    return {};
                }
            }

            const std::error_category* get_category(int facility) const
            {
                const std::shared_lock<std::shared_mutex> lock{ m_mutex };

                const auto iter = m_storageBimap.right().find(facility);
                if (iter != m_storageBimap.right().end())
                {
                    return iter->second;
                }
                else
                {
                    return nullptr;
                }
            }

        private:
            mutable std::shared_mutex m_mutex;
            int m_categoryFacility{ 0 };
            unordered_bimap<const std::error_category*, int32_t> m_storageBimap;
        };

        static std::optional<category_storage> s_categoryStorage;
    }

    category_storage& get_category_storage()
    {
        if (!s_categoryStorage.has_value())
        {
            s_categoryStorage.emplace();
        }

        return *s_categoryStorage;
    }

    const std::error_category* get_category_from_hresult(int32_t hresult)
    {
        // If the customer bit is not set, this is by definition a normal HRESULT and
        // not one of our custom ones.
        if ((hresult & CustomerBitMask) == 0)
        {
            return nullptr;
        }

        return get_category_storage().get_category(HRESULT_FACILITY(hresult));
    }

    const char* hresult_error_category::name() const noexcept
    {
        return "hresult_error_category";
    }

    std::string hresult_error_category::message(int evt) const
    {
        const winrt::hresult_error err{ evt };

        const std::string converted = utf16_to_utf8(err.message().c_str());
        if (converted.empty())
        {
            return "Unknown HRESULT";
        }
        else
        {
            return converted;
        }
    }

    void hresult_error_category::add_category(const std::error_category& category)
    {
        get_category_storage().add_category(category);
    }

    const std::error_category& hresult_category()
    {
        static hresult_error_category cat;
        return cat;
    }

    std::error_code error_code_from_hr(hresult hresult)
    {
        return error_code_from_hr(underlying_cast(hresult));
    }

    std::error_code error_code_from_hr(int32_t hresult)
    {
        if (const std::error_category* category = get_category_from_hresult(hresult))
        {
            return { HRESULT_CODE(hresult), *category };
        }
        else
        {
            return { hresult, hresult_category() };
        }
    }

    int32_t hr_from_error_code(const std::error_code& error_code)
    {
        // Make sure we're being asked to convert an actual error.
        assert(static_cast<bool>(error_code));

        if (error_code.category() == hresult_category())
        {
            // already an hresult - no need to modify
            return error_code.value();
        }

        assert((error_code.value() == HRESULT_CODE(error_code.value())) && "error_code value using more than 16 bits, which is too large for an hresult");

        std::optional<int> facility = get_category_storage().get_facility(error_code.category());
        if (facility)
        {
            return make_hresult_code(facility.value(), error_code.value());
        }
        else
        {
            assert(false && "Unexpected error category");
            return arcana::underlying_cast(hresult::e_fail);
        }
    }
}
