#include <gtest/gtest.h>

#include <arcana/functional/inplace_function.h>
#include <arcana/messaging/mediator.h>
#include <arcana/expected.h>

#include <fstream>

namespace
{
    struct tracer
    {
        tracer(std::ofstream& stream)
            : m_stream{ &stream }
        {}

        tracer()
            : m_stream{ nullptr }
        {}

        tracer(const tracer& other)
            : m_stream{ other.m_stream }
        {
            *m_stream << "T(const T&)" << std::endl;
        }

        tracer(tracer&& other)
            : m_stream{ other.m_stream }
        {
            *m_stream << "T(T&&)" << std::endl;
            other.m_stream = nullptr;
        }

        tracer& operator =(const tracer& other)
        {
            *m_stream << "T& operator=(const T&)" << std::endl;
            m_stream = other.m_stream;
            return *this;
        }

        tracer& operator =(tracer&& other)
        {
            *m_stream << "T& operator=(T&&)" << std::endl;
            m_stream = other.m_stream;
            other.m_stream = nullptr;
            return *this;
        }

        std::ofstream* m_stream;
    };

    static void call(std::shared_ptr<int> ptr, std::unique_ptr<int> unique)
    {
        std::ofstream blah{ "output.txt" };
        blah << "shared: " << *ptr << " unique: " << *unique << std::endl;
    }

    static void trace_value(tracer t)
    {
        *t.m_stream << "done" << std::endl;
    }

    static void trace_ref(tracer& t)
    {
        *t.m_stream << "done" << std::endl;
    }

    static void trace_mov(tracer&& t)
    {
        *t.m_stream << "done" << std::endl;
    }
}

TEST(MediatorUnitTest, InplaceFunctionForwardingSemantics)
{
    std::ofstream file{ "tracing.txt", std::ios::trunc };

    {
        file << "trace_value1" << std::endl;
        stdext::inplace_function<void(tracer)> func{ &trace_value };
        func(tracer{ file });
    }

    {
        file << "trace_value2" << std::endl;
        stdext::inplace_function<void(tracer)> func{ &trace_value };
        tracer t{ file };
        func(t);
    }

    {
        file << "trace_value3" << std::endl;
        stdext::inplace_function<void(tracer)> func{ &trace_value };
        tracer t{ file };
        func(std::move(t));
    }

    {
        file << "trace_ref" << std::endl;
        stdext::inplace_function<void(tracer&)> func{ &trace_ref };
        tracer t{ file };
        func(t);
    }

    {
        file << "trace_mov" << std::endl;
        stdext::inplace_function<void(tracer&&)> func{ &trace_mov };
        func(tracer{ file });
    }

    {
        stdext::inplace_function<void(std::shared_ptr<int>, std::unique_ptr<int>)> func{ &call };
        func(std::make_shared<int>(0), std::make_unique<int>(1));
    }
}

struct one
{
    int value{};
};

struct two
{
    std::string message{};
};

struct three
{
    int mat[3]{};
};

TEST(MediatorUnitTest, RouterSingleEvent)
{
    arcana::router<one> rout;

    int received = 10;
    auto reg1 = rout.add_listener<one>([&received](const one& evt)
    {
        received = evt.value;
    });

    rout.fire(one{ 1 });

    EXPECT_EQ(1, received);
}

TEST(MediatorUnitTest, RouterNonPODType)
{
    arcana::router<arcana::expected<std::shared_ptr<int>, std::error_code>> rout;

    auto lambda = std::make_shared<int>(0);
    std::weak_ptr<int> wlambda = lambda;
    std::weak_ptr<int> wshared;

    int received = 0;

    {
        auto reg1 = rout.add_listener<arcana::expected<std::shared_ptr<int>, std::error_code>>(
            [&received, lambda = std::move(lambda)](const arcana::expected<std::shared_ptr<int>, std::error_code>& evt)
            {
                received = *evt.value();
            });

        auto shared = std::make_shared<int>(10);
        wshared = shared;

        rout.fire(arcana::expected<std::shared_ptr<int>, std::error_code>{ std::move(shared) });
    }

    EXPECT_EQ(10, received);
    EXPECT_TRUE(wshared.expired());
    EXPECT_TRUE(wlambda.expired());
}

TEST(MediatorUnitTest, RouterMultipleEvent)
{
    arcana::router<one, two> rout;

    int received = 10;
    auto reg1 = rout.add_listener<one>([&received](const one& /*evt*/)
    {
        received = 1;
    });

    auto reg2 = rout.add_listener<two>([&received](const two& /*evt*/)
    {
        received = 2;
    });

    rout.fire(one{});

    EXPECT_EQ(1, received);

    rout.fire(one{});

    EXPECT_EQ(1, received);

    rout.fire(two{});

    EXPECT_EQ(2, received);

    rout.fire(one{});

    EXPECT_EQ(1, received);
}

TEST(MediatorUnitTest, RouterUnregister)
{
    arcana::router<one, two> rout;

    int received = 0;
    {
        auto reg = rout.add_listener<one>([&](const one& /*evt*/)
        {
            received++;
        });

        rout.fire(one{});
    }
    rout.fire(one{});
    EXPECT_EQ(1, received);

    {
        auto reg = rout.add_listener<one>([&](const one& /*evt*/)
        {
            received++;
        });

        rout.fire(one{});
    }
    EXPECT_EQ(2, received);

    rout.fire(one{});
    EXPECT_EQ(2, received);

    std::unique_ptr<arcana::ticket> ticket;
    ticket = std::make_unique<arcana::ticket>(rout.add_listener<one>([&](const one& /*evt*/)
    {
        received++;

        ticket.reset();
    }));
    rout.fire(one{});
    EXPECT_EQ(3, received);
    rout.fire(one{});
    EXPECT_EQ(3, received);

    std::unique_ptr<arcana::ticket> ticket2;
    ticket2 = std::make_unique<arcana::ticket>(rout.add_listener<one>([&](const one& /*evt*/)
    {
        received++;

        ticket2.reset();

        rout.fire(one{});
    }));

    std::unique_ptr<arcana::ticket> ticket3;
    ticket3 = std::make_unique<arcana::ticket>(rout.add_listener<one>([&](const one& /*evt*/)
    {
        received *= 2;

        ticket3.reset();

        rout.fire(one{});
    }));

    rout.fire(one{});

    EXPECT_EQ(8, received);
}

TEST(MediatorUnitTest, RouterRegisterOther)
{
    arcana::router<one, two> rout;

    int received = 0;
    std::unique_ptr<arcana::ticket> twol;
    auto reg = rout.add_listener<one>([&](const one& /*evt*/)
    {
        received++;

        twol = std::make_unique<arcana::ticket>(rout.add_listener<two>([&](const two& /*other*/)
        {
            received *= 2;
        }));

        rout.fire(two{});
    });

    rout.fire(one{});
    EXPECT_EQ(2, received);

    rout.fire(two{});
    EXPECT_EQ(4, received);
}

TEST(MediatorUnitTest, RouterRegisterSame)
{
    arcana::router<one, two> rout;

    int received = 0;
    std::unique_ptr<arcana::ticket> twol, onel;
    auto reg = std::make_unique<arcana::ticket>(rout.add_listener<one>([&](const one& /*evt*/)
    {
        received++;

        twol = std::make_unique<arcana::ticket>(rout.add_listener<two>([&](const two& /*other*/)
        {
            received *= 2;
        }));

        rout.fire(two{});

        onel = std::make_unique<arcana::ticket>(rout.add_listener<one>([&](const one& /*otherone*/)
        {
            received += 7;
        }));
    }));

    rout.fire(one{});
    EXPECT_EQ(2, received);

    reg.reset();

    rout.fire(one{});
    EXPECT_EQ(9, received);
}

TEST(MediatorUnitTest, RouterFire)
{
    arcana::router<one, two> rout;

    int received = 0;
    auto oreg = rout.add_listener<one>([&](const one& /*evt*/)
    {
        received++;

        rout.fire(two{});
    });

    {
        auto treg = rout.add_listener<two>([&](const two& /*evt*/)
        {
            received *= 2;
        });

        rout.fire(one{});

        EXPECT_EQ(2, received);

        rout.fire(two{});
        EXPECT_EQ(4, received);
    }

    rout.fire(one{});

    EXPECT_EQ(5, received);

    std::unique_ptr<arcana::ticket> treg;
    treg = std::make_unique<arcana::ticket>(rout.add_listener<two>([&](const two& /*evt*/)
    {
        received *= 2;

        treg.reset();

        rout.fire(one{});
    }));

    rout.fire(one{});

    EXPECT_EQ(13, received);
}

TEST(MediatorUnitTest, DispatcherOrdering)
{
    arcana::manual_dispatcher<32> dis;
    dis.set_affinity({ std::this_thread::get_id() });

    int value = -1;
    dis.queue([&]
    {
        value = 1;
    });

    dis.queue([&]
    {
        value *= 2;
    });

    dis.queue([&]
    {
        value -= 5;
    });

    EXPECT_EQ(-1, value);

    arcana::cancellation_source source;
    dis.tick(source);

    EXPECT_EQ(-3, value);
}

TEST(MediatorUnitTest, DispatcherOrderingRecursive)
{
    arcana::manual_dispatcher<32> dis;
    dis.set_affinity({ std::this_thread::get_id() });

    int value = -1;
    dis.queue([&]
    {
        value = 1;

        dis.queue([&]
        {
            value *= 5;
        });
    });

    dis.queue([&]
    {
        value *= 2;

        dis.queue([&]
        {
            value -= 3;
        });
    });

    dis.queue([&]
    {
        value -= 5;

        dis.queue([&]
        {
            value *= -2;
        });
    });

    EXPECT_EQ(-1, value);

    arcana::cancellation_source source;
    dis.tick(source);

    EXPECT_EQ(-3, value);

    dis.tick(source);

    EXPECT_EQ(36, value);
}

TEST(MediatorUnitTest, SingleEvent)
{
    arcana::manual_dispatcher<32> dis;
    arcana::mediator<decltype(dis), one> med{ dis };

    int received = 10;
    auto reg = med.add_listener<one>([&received](const one& evt)
    {
        received = evt.value;
    });

    med.send(one{ 1 });

    arcana::cancellation_source source;
    dis.tick(source);

    EXPECT_EQ(1, received);
}

TEST(MediatorUnitTest, MediatorNonPODType)
{
    arcana::manual_dispatcher<32> dis;
    arcana::mediator<decltype(dis), arcana::expected<std::shared_ptr<int>, std::error_code>> rout{ dis };

    auto lambda = std::make_shared<int>(0);
    std::weak_ptr<int> wlambda = lambda;
    std::weak_ptr<int> wshared;

    int received = 0;

    {
        auto reg1 = rout.add_listener<arcana::expected<std::shared_ptr<int>, std::error_code>>(
            [&received, lambda = std::move(lambda)](const arcana::expected<std::shared_ptr<int>, std::error_code>& evt)
            {
                received = *evt.value();
            });

        auto shared = std::make_shared<int>(10);
        wshared = shared;

        rout.send(arcana::expected<std::shared_ptr<int>, std::error_code>{ std::move(shared) });

        arcana::cancellation_source source;
        dis.tick(source);
    }

    EXPECT_EQ(10, received);
    EXPECT_TRUE(wshared.expired());
    EXPECT_TRUE(wlambda.expired());
}

TEST(MediatorUnitTest, MultipleEvents)
{
    arcana::manual_dispatcher<64> dis; // Manual dispatcher size is selected to be big enough to fit pointer to mediator and maximal size out of "one", "two" and "three" below.
    arcana::mediator<decltype(dis), one, two, three> med{ dis };
    arcana::ticket_scope registrations;

    int received = 10;
    registrations += med.add_listener<one>([&received](const one& evt)
    {
        received = evt.value;
    });

    registrations += med.add_listener<two>([&received](const two& evt)
    {
        for (char c : evt.message)
        {
            received *= c;
        }
    });

    registrations += med.add_listener<three>([&received](const three& evt)
    {
        for (int c : evt.mat)
        {
            received += c;
        }
    });

    med.send(one{ 3 });
    med.send(two{ "two" });
    med.send(three{ {1, 2, 3} });

    arcana::cancellation_source source;
    dis.tick(source);

    EXPECT_EQ(4596738, received);
}
