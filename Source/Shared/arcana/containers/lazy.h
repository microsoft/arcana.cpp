#pragma once

#include <memory>
#include <tuple>

namespace arcana
{
    template<typename T, typename... Ts>
    class lazy
    {
    public:
        lazy(Ts... args)
            : m_args{std::make_unique<std::tuple<Ts...>>(std::make_tuple(std::move(args)...))}
        {
        }

        operator T&()
        {
            if (m_value == nullptr)
            {
                auto initializer = [this](Ts... args) { m_value = std::make_unique<T>(std::move(args)...); };
                std::apply(initializer, *m_args);
                m_args.reset();
            }

            return *m_value;
        }

    private:
        std::unique_ptr<T> m_value{};
        std::unique_ptr<std::tuple<Ts...>> m_args{};
    };

    template<typename T, typename... Ts>
    lazy<T, Ts...> make_lazy(Ts... args)
    {
        return {std::move(args)...};
    }
}
