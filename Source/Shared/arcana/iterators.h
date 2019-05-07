#pragma once

#include <iterator>
#include <tuple>
#include <utility>

namespace arcana
{
    template<size_t N>
    struct static_for_iterator
    {
        using indexing = std::make_index_sequence<N>;

        template<typename IteratorT>
        static void iterate(IteratorT&& iterator)
        {
            iterate(std::forward<IteratorT>(iterator), indexing{});
        }

    private:
        template<typename IteratorT, size_t... Index>
        static void iterate(IteratorT&& iterator, std::integer_sequence<size_t, Index...>)
        {
            int unused[] = { (iterator(std::integral_constant<size_t, Index>{}), 0)... };
            (void)unused;
        }
    };

    template<>
    struct static_for_iterator<0>
    {
        template<typename IteratorT>
        static void iterate(IteratorT&&)
        {}
    };

    template<size_t N, typename IteratorT>
    void static_for(IteratorT&& iterator)
    {
        static_for_iterator<N>::iterate(std::forward<IteratorT>(iterator));
    }

    template<typename IteratorT, typename ...ArgTs>
    void static_foreach(IteratorT&& iterator, ArgTs&& ...args)
    {
        int unused[] = { (iterator(std::forward<ArgTs>(args)), 0)..., 0 };
        (void)iterator; // in case of empty set
        (void)unused;
    }

    template<typename TupleT, typename IteratorT>
    void iterate_tuple(TupleT&& tuple, IteratorT&& iterator)
    {
        static_for<std::tuple_size<std::decay_t<TupleT>>::value>([&iterator, &tuple](auto index)
        {
            iterator(std::get<index>(tuple), index);
        });
    }
}
