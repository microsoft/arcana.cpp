//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <unordered_map>
#include <utility>

namespace arcana
{
    template<typename TLeft, typename TRight>
    class unordered_bimap
    {
    public:
        const std::unordered_map<TLeft, TRight>& left() const
        {
            return m_leftToRight;
        }

        const std::unordered_map<TRight, TLeft>& right() const
        {
            return m_rightToLeft;
        }

        void emplace(TLeft&& left, TRight&& right)
        {
            m_leftToRight.emplace(left, right);
            m_rightToLeft.emplace(std::forward<TRight>(right), std::forward<TLeft>(left));
        }

    private:
        std::unordered_map<TLeft, TRight> m_leftToRight;
        std::unordered_map<TRight, TLeft> m_rightToLeft;
    };
}
