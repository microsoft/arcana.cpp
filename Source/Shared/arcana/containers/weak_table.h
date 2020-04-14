#pragma once

#include <map>

namespace arcana
{
    // NOTE: This type is not thread-safe.
    template<typename T>
    class weak_table
    {
        // NOTE: Philosophically, this type is actually std::map<map_t**, T>.
        // That's a recursive type, though, so for simplicity's sake we use
        // void instead of map_t in the definition here.
        using map_t = std::map<void**, T>;

    public:
        class ticket
        {
        public:
            ticket(const ticket&) = delete;

            ticket(ticket&& other)
                : m_collection{ other.m_collection }
            {
                other.m_collection = nullptr;
            }

            ~ticket()
            {
                // If m_collection itself is a nullptr, then the object being
                // destructed is the "empty shell" left over after the use of 
                // a move constructor has been used to logically move the 
                // ticket. In this case, there's nothing the destructor needs
                // to do, so early-out.
                if (m_collection == nullptr)
                {
                    return;
                }

                map_t* ptr = *m_collection;
                if (ptr != nullptr)
                {
                    ptr->erase(reinterpret_cast<void**>(m_collection));
                }

                delete m_collection;
            }

        private:
            friend class weak_table;

            ticket(T&& value, map_t& collection)
                : m_collection{ new map_t*(&collection) }
            {
                collection[reinterpret_cast<void**>(m_collection)] = std::move(value);
            }

            map_t** m_collection;
        };

        weak_table() = default;
        weak_table(const weak_table&) = delete;
        weak_table(weak_table&&) = delete;

        ~weak_table()
        {
            clear();
        }

        ticket insert(T&& value)
        {
            return{ std::move(value), m_map };
        }

        template<typename CallableT>
        void apply_to_all(CallableT callable)
        {
            for (auto& [ptr, value] : m_map)
            {
                callable(value);
            }
        }

        void clear()
        {
            for (auto& [ptr, value] : m_map)
            {
                *ptr = nullptr;
            }

            m_map.clear();
        }

    private:
        map_t m_map{};
    };
}
