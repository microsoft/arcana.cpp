#pragma once

#include <optional>
#include <thread>

namespace arcana
{
    class affinity
    {
    public:
        affinity(const std::thread::id& id)
            : m_thread{ id }
        {}

        affinity()
        {}

        bool check() const
        {
            if (!m_thread)
                return true;

            return std::this_thread::get_id() == m_thread;
        }

        bool is_set() const
        {
            return m_thread.has_value();
        }

    private:
        std::optional<std::thread::id> m_thread;
    };
}
