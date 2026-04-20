#pragma once

#include <cassert>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace arcana
{
    // NOTE: This type is not thread-safe.
    template<typename T>
    class weak_table
    {
    public:
        class ticket
        {
        public:
            ticket(const ticket&) = delete;

            ticket(ticket&& other) noexcept
                : m_table{ other.m_table }
            {
                other.m_table = nullptr;
            }
            ticket& operator=(const ticket&) = delete;
            ticket& operator=(ticket&&) = delete;

            ~ticket()
            {
                // If m_table itself is a nullptr, then the object being
                // destructed is the "empty shell" left over after the use of 
                // a move constructor has been used to logically move the 
                // ticket. In this case, there's nothing the destructor needs
                // to do, so early-out.
                if (m_table == nullptr)
                {
                    return;
                }

                auto* table = *m_table;
                if (table != nullptr)
                {
                    table->internal_erase(m_table);
                }

                delete m_table;
            }

            struct hash : private std::hash<weak_table**>
            {
                auto operator()(const ticket& ticket) const
                {
                    return std::hash<weak_table**>::operator()(ticket.m_table);
                }
            };

            bool operator==(const ticket& other) const
            {
                assert(other.m_table != m_table);
                return false;
            }

        private:
            friend class weak_table;
            weak_table** m_table{};

            template<typename... Ts>
            ticket(weak_table& table, Ts &&...args)
                : m_table{ new weak_table * (&table) }
            {
                table.internal_insert(m_table, std::forward<Ts>(args)...);
            }
        };

        weak_table() = default;
        weak_table(const weak_table&) = delete;
        weak_table(weak_table&&) = delete;

        ~weak_table()
        {
            clear();
        }

        template<typename... Ts>
        ticket insert(Ts &&...args)
        {
            return { *this, std::forward<Ts>(args)... };
        }

        template<typename CallableT, typename = std::enable_if_t<std::is_same_v<std::invoke_result_t<CallableT, T&>, bool>>>
        void apply_to_each_while_true(CallableT&& callable)
        {
            internal_apply(std::forward<CallableT>(callable));
        }

        template<typename CallableT, typename = std::enable_if_t<std::is_same_v<std::invoke_result_t<CallableT, T&>, void>>>
        void apply_to_all(CallableT&& callable)
        {
            internal_apply(std::forward<CallableT>(callable));
        }

        void clear()
        {
            for (auto& [key, value] : m_collection)
            {
                *key = nullptr;
            }

            m_collection.clear();
        }

    private:
        std::unordered_map<weak_table**, std::optional<T>> m_collection{};
        std::unordered_map<weak_table**, std::optional<T>> m_insertions{};
        std::unordered_set<weak_table**> m_deletions{};
        bool m_applying{ false };
        weak_table** m_applyingKey{ nullptr };
        bool m_shouldResetAfterApplying{ false };

        friend class ticket;

        template <typename CallableT>
        void internal_apply(CallableT&& callable)
        {
            // internal_apply() is never allowed to recurse
            assert(!m_applying);
            m_applying = true;

            bool shouldContinue = true;
            for (auto& [key, value] : m_collection)
            {
                m_applyingKey = key;
                m_shouldResetAfterApplying = false;

                // If the value variable is empty, it is considered erased
                if (value.has_value())
                {
                    if constexpr (std::is_same_v<std::invoke_result_t<CallableT, T&>, bool>)
                    {
                        shouldContinue = callable(value.value());
                    }
                    else
                    {
                        callable(value.value());
                    }
                }

                // m_shouldResetAfterApplying can only be true if it was set by an operation,
                // inside callable(), which indicates that callable() destroyed the ticket
                // which was responsible for the lifespan of this value. It was unsafe to
                // destroy the data at that time because it was actively being processed, so
                // we do so now.
                if (m_shouldResetAfterApplying)
                {
                    value.reset();
                }

                if (!shouldContinue)
                {
                    break;
                }
            }
            m_applyingKey = nullptr;

            for (weak_table** deletedKey : m_deletions)
            {
                // Everything in m_deletions should have already been reset, so none of these
                // elemeents to be removed from the collection should currently have a value.
                assert(!m_collection[deletedKey].has_value());
                m_collection.erase(deletedKey);
            }
            m_deletions.clear();

            if (!m_insertions.empty())
            {
                std::unordered_map<weak_table**, std::optional<T>> insertions{};
                insertions.swap(m_insertions);
                m_collection.merge(std::move(insertions));
            }

            m_applying = false;
        }

        template <typename... Ts>
        void internal_insert(weak_table** key, Ts &&...args)
        {
            // If we're actively applying, that means internal_apply() is somewhere above us on the
            // stack, and it is consequently not safe to modify m_collection itself directly, so
            // for that case we insert into a separate collection which will be merged with
            // m_collection once it is safe to do so.
            if (m_applying)
            {
                m_insertions.try_emplace(key, std::make_optional<T>(std::forward<Ts>(args)...));
            }
            else
            {
                m_collection.try_emplace(key, std::make_optional<T>(std::forward<Ts>(args)...));
            }
        }

        void internal_erase(weak_table** key)
        {
            if (m_applying)
            {
                // If we're actively applying, that means internal_apply() is somewhere above us on the
                // stack, and it is consequently not safe to modify m_collection itself directly.
                if (key == m_applyingKey)
                {
                    // In this case, we are trying to erase the very element which is currently being
                    // used in internal_apply() higher in the stack, and it is unsafe to erase this data.
                    // Instead, set the m_shouldResetAfterApplying flag to let internal_apply() know to
                    // erase this data as soon as it is safe to do so -- i.e., when that frame is once
                    // again on top of the stack.
                    m_shouldResetAfterApplying = true;
                }
                else
                {
                    // In this case, the iteration in internal_apply() is currently processing a different
                    // element, so it is safe to erase the data, though not to remove it from the
                    // collection.
                    m_collection[key].reset();
                }
                // Add this key to the list of elements for internal_apply() to delete once it is safe
                // to do so.
                m_deletions.insert(key);
            }
            else
            {
                // internal_apply() is not running elsewhere, so it is safe to modify the collection
                // directly.
                m_collection.erase(key);
            }
        }
    };
}
