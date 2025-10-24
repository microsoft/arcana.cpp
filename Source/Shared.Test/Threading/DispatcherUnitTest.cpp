#include <gtest/gtest.h>

#include <arcana/threading/dispatcher.h>

#include <numeric>
#include <algorithm>
#include <future>

TEST(DispatcherUnitTest, DispatcherLeakCheck)
{
    std::weak_ptr<int> weak;

    {
        arcana::background_dispatcher<32> dis;

        std::shared_ptr<int> strong = std::make_shared<int>(10);
        weak = strong;

        std::promise<void> signal;

        dis.queue([strong, &signal]{
            signal.set_value();
        });

        signal.get_future().get();
    }

    EXPECT_TRUE(weak.expired());
}
