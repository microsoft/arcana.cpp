#include <CppUnitTest.h>

#include <arcana\scheduling\state_machine.h>

#include <arcana\threading\dispatcher.h>
#include <arcana\threading\pending_task_scope.h>

#include <arcana\messaging\mediator.h>

#include <memory>
#include <future>

using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace UnitTests
{
    arcana::state_machine_state<bool> TrackingInit{ "TrackingInit" };

    arcana::state_machine_state<void> TrackingRead{ "TrackingRead" };
    arcana::state_machine_state<void> TrackingWrite{ "TrackingWrite" };

    struct ImageReceived
    {};

    using Mediator = arcana::mediator<arcana::dispatcher<32>, ImageReceived>;

    struct Context
    {
        Mediator& Mediator;
        arcana::state_machine_observer& StateMachine;
        arcana::dispatcher<32>& Dispatcher;
    };

    struct InitializationWorker
    {
        InitializationWorker(Context& context)
            : m_context{ context }
        {
            m_registrations += m_context.Mediator.add_listener<ImageReceived>([this](auto i) { Run(i); });
        }

        void Run(const ImageReceived&)
        {
            m_pending += m_context.StateMachine.on(TrackingInit, m_context.Dispatcher, m_cancel, [&](bool& result) noexcept
            {
                Count++;

                if (Count > 3)
                {
                    m_registrations.clear();
                    result = true;
                }
            });
        }
        
        arcana::task<void, std::error_code> ShutdownAsync()
        {
            m_cancel.cancel();
            return m_pending.when_all();
        }

        int Count{};

    private:
        Context& m_context;
        arcana::ticket_scope m_registrations;
        arcana::pending_task_scope<std::error_code> m_pending;
        arcana::cancellation_source m_cancel;
    };

    struct TrackingWorker
    {
        TrackingWorker(Context& context)
            : m_context{ context }
        {
            m_registrations += m_context.Mediator.add_listener<ImageReceived>([this](auto i) { Run(i); });
        }

        void Run(const ImageReceived&)
        {
            if (m_cancel.cancelled())
                return;

            m_previous = m_previous.then(m_context.Dispatcher, m_cancel, [&]() noexcept
            {
                return m_context.StateMachine.on(TrackingRead, m_context.Dispatcher, m_cancel, [&]() noexcept
                {
                    return Result + 10;
                });
            }).then(m_context.Dispatcher, m_cancel, [&](int value) noexcept
            {
                value += 30;

                return m_context.StateMachine.on(TrackingWrite, m_context.Dispatcher, m_cancel, [this, value]() noexcept
                {
                    Result += value;
                    Iterations++;
                });
            });

            m_scope += m_previous;
        }

        arcana::task<void, std::error_code> ShutdownAsync()
        {
            m_cancel.cancel();

            return m_scope.when_all();
        }

        int Iterations{};
        int Result{};

    private:
        arcana::task<void, std::error_code> m_previous = arcana::task_from_result<std::error_code>(arcana::expected<void, std::error_code>::make_valid());
        Context& m_context;
        arcana::ticket_scope m_registrations;
        arcana::pending_task_scope<std::error_code> m_scope;
        arcana::cancellation_source m_cancel;
    };

    arcana::task<void, std::error_code> LinearSchedule(arcana::state_machine_driver& driver, arcana::dispatcher<32>& dispatcher)
    {
        return arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
        {
            // do a tracking read
            return driver.move_to(TrackingRead, arcana::cancellation::none());
        }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
        {
            // then a tracking write
            return driver.move_to(TrackingWrite, arcana::cancellation::none());
        }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
        {
            // then repeat the current schedule
            return LinearSchedule(driver, dispatcher);
        });
    }

    arcana::task<void, std::error_code> InitializationSchedule(arcana::state_machine_driver& driver, arcana::dispatcher<32>& dispatcher)
    {
        return make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
        {
            return driver.move_to(TrackingInit, arcana::cancellation::none());
        }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&](bool initialized) noexcept
        {
            if (initialized)
            {
                return LinearSchedule(driver, dispatcher);
            }
            else
            {
                // then repeat the current schedule
                return InitializationSchedule(driver, dispatcher);
            }
        });
    }

    TEST_CLASS(SchedulingUnitTest)
    {
        TEST_METHOD(RepeatingLinearSchedule)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };
            arcana::manual_dispatcher<32> dispatch;
            Mediator mediator{ dispatch };

            LinearSchedule(driver, dispatch);

            Context context{
                mediator,
                sched,
                dispatch
            };

            TrackingWorker worker{ context };

            do
            {
                if (worker.Iterations == 2)
                {
                    worker.ShutdownAsync();
                    break;
                }

                mediator.send<ImageReceived>({});
            } while (dispatch.tick(arcana::cancellation::none()));

            while (dispatch.tick(arcana::cancellation::none())) {};

            Assert::AreEqual(120, worker.Result);
        }

        TEST_METHOD(ConditionalSchedule)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };
            arcana::manual_dispatcher<32> dispatch;
            Mediator mediator{ dispatch };

            InitializationSchedule(driver, dispatch);

            Context context{
                mediator,
                sched,
                dispatch
            };

            InitializationWorker init{ context };
            TrackingWorker worker{ context };

            do
            {
                if (worker.Iterations == 2)
                {
                    worker.ShutdownAsync();
                    break;
                }

                mediator.send<ImageReceived>({});
            } while (dispatch.tick(arcana::cancellation::none()));

            while (dispatch.tick(arcana::cancellation::none())) {};

            Assert::AreEqual(4, init.Count);
            Assert::AreEqual(2, worker.Iterations);
            Assert::AreEqual(120, worker.Result);
        }

        TEST_METHOD(SendDataFromWorker)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };

            arcana::state_machine_state<bool> one{ "One" }, two{ "Two" }, three{ "Three" };

            arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const bool& data) noexcept
            {
                Assert::IsFalse(data);
                return driver.move_to(two, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const bool& data) noexcept
            {
                Assert::IsTrue(data);
                return driver.move_to(three, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const bool& data) noexcept
            {
                Assert::IsFalse(data);
            });

            std::stringstream ss;

            arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return sched.on(one, arcana::inline_scheduler, arcana::cancellation::none(), [&](bool&) noexcept
                {
                    ss << "one";
                });
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return sched.on(two, arcana::inline_scheduler, arcana::cancellation::none(), [&](bool& data) noexcept
                {
                    ss << "two";
                    data = true;
                });
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return sched.on(three, arcana::inline_scheduler, arcana::cancellation::none(), [&](bool&) noexcept
                {
                    ss << "three";
                });
            });

            Assert::AreEqual<std::string>("onetwothree", ss.str());
        }

        TEST_METHOD(MoveToEachState)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };
            
            arcana::state_machine_state<void> one{ "One" }, two{ "Two" }, three{ "Three" };

            arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(two, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(three, arcana::cancellation::none());
            });

            std::stringstream ss;

            arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return sched.on(one, arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    ss << "one";
                });
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return sched.on(two, arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    ss << "two";
                });
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return sched.on(three, arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    ss << "three";
                });
            });

            Assert::AreEqual<std::string>("onetwothree", ss.str());
        }

        TEST_METHOD(CancellationCancelsTheSchedulingMethod)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };

            arcana::state_machine_state<void> one{ "One" }, two{ "Two" };

            arcana::cancellation_source cancel;

            bool driverFinished = false;

            arcana::make_task(arcana::inline_scheduler, cancel, [&]() noexcept
            {
                return driver.move_to(one, cancel);
            }).then(arcana::inline_scheduler, cancel, [&]() noexcept
            {
                return driver.move_to(two, cancel);
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>&) noexcept
            {
                driverFinished = true;
            });

            bool observerFinished = false;

            arcana::make_task(arcana::inline_scheduler, cancel, [&]() noexcept
            {
                cancel.cancel();
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>&) noexcept
            {
                observerFinished = true;
            });

            Assert::IsTrue(driverFinished);
            Assert::IsTrue(observerFinished);
        }

        arcana::task<void, std::error_code> WorkOn(
            std::stringstream& stream,
            arcana::state_machine_observer& sched,
            arcana::state_machine_state<void>& state,
            arcana::cancellation& cancel,
            arcana::dispatcher<32>& dispatcher)
        {
            return arcana::make_task(dispatcher, cancel, [&]() noexcept
            {
                return sched.on(state, dispatcher, cancel, [&]() noexcept
                {
                    stream << state.name();
                });
            }).then(dispatcher, cancel, [&]() noexcept
            {
                return WorkOn(stream, sched, state, cancel, dispatcher);
            });
        }

        TEST_METHOD(SequentialSchedule)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };
            arcana::cancellation_source cancel;
            arcana::background_dispatcher<32> background;

            arcana::state_machine_state<void> one{ "1" }, two{ "2" }, three{ "3" };

            std::promise<void> signal;

            arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(two, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(three, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                signal.set_value();
            });

            std::stringstream ss;

            WorkOn(ss, sched, one, cancel, background);
            WorkOn(ss, sched, two, cancel, background);
            WorkOn(ss, sched, three, cancel, background);

            signal.get_future().get();
            cancel.cancel();

            Assert::AreEqual<std::string>("123", ss.str());
        }

        TEST_METHOD(InvertSequentialSchedule)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };
            arcana::cancellation_source cancel;
            arcana::background_dispatcher<32> background;

            arcana::state_machine_state<void> one{ "1" }, two{ "2" }, three{ "3" };

            std::promise<void> signal;

            std::stringstream ss;

            WorkOn(ss, sched, one, cancel, background);
            WorkOn(ss, sched, two, cancel, background);
            WorkOn(ss, sched, three, cancel, background);

            arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(two, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(three, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                signal.set_value();
            });

            signal.get_future().get();
            cancel.cancel();

            Assert::AreEqual<std::string>("123", ss.str());
        }

        TEST_METHOD(LoopSchedule)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };
            arcana::cancellation_source cancel;
            arcana::background_dispatcher<32> background;

            arcana::state_machine_state<void> one{ "1" }, two{ "2" }, three{ "3" };

            std::promise<void> signal;

            make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(two, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(three, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(two, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(three, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                signal.set_value();
            });

            std::stringstream ss;

            WorkOn(ss, sched, one, cancel, background);
            WorkOn(ss, sched, two, cancel, background);
            WorkOn(ss, sched, three, cancel, background);

            signal.get_future().get();
            cancel.cancel();

            Assert::AreEqual<std::string>("123123", ss.str());
        }

        TEST_METHOD(JumpAroundTheGraph)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };
            arcana::cancellation_source cancel;
            arcana::background_dispatcher<32> background;

            arcana::state_machine_state<void> one{ "1" }, two{ "2" }, three{ "3" };

            std::promise<void> signal;

            arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(three, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(two, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(three, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                signal.set_value();
            });

            std::stringstream ss;

            WorkOn(ss, sched, one, cancel, background);
            WorkOn(ss, sched, two, cancel, background);
            WorkOn(ss, sched, three, cancel, background);

            signal.get_future().get();
            cancel.cancel();

            Assert::AreEqual<std::string>("1311213", ss.str());
        }

        TEST_METHOD(CancelInOnMethodDoesntBlockSchedule)
        {
            arcana::state_machine_driver driver{};
            arcana::state_machine_observer sched{ driver };

            arcana::state_machine_state<void> one{ "One" }, two{ "Two" };
            arcana::cancellation_source cancel;

            bool ranContinuation = false;

            arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return driver.move_to(one, arcana::cancellation::none());
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                ranContinuation = true;
            });

            make_task(arcana::inline_scheduler, cancel, [&]() noexcept
            {
                return sched.on(one, arcana::inline_scheduler, cancel, [&]() noexcept
                {
                    cancel.cancel();
                });
            });

            Assert::IsTrue(ranContinuation);
        }

        struct Runtime
        {
            arcana::state_machine_driver Driver{};
            arcana::state_machine_observer Scheduler{ Driver };
            arcana::manual_dispatcher<32> Dispatcher;
            Mediator Mediator{ Dispatcher };

            Context Context{ Mediator, Scheduler, Dispatcher };

            std::unique_ptr<InitializationWorker> m_init;
            std::unique_ptr<TrackingWorker> m_tracking;

            arcana::task<void, std::error_code> Start()
            {
                return InitSchedule();
            }

            bool FailedOnce = false;
            bool Completed = false;

            arcana::task<void, std::error_code> Die()
            {
                return arcana::task_from_error<void>(std::errc::owner_dead);
            }

            arcana::task<void, std::error_code> TrackingSchedule()
            {
                if (!m_tracking)
                {
                    m_tracking = std::make_unique<TrackingWorker>(Context);
                }

                return arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    // do a tracking read
                    return Driver.move_to(TrackingRead, arcana::cancellation::none());
                }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    // then a tracking write
                    return Driver.move_to(TrackingWrite, arcana::cancellation::none());
                }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    if (!FailedOnce && m_tracking->Iterations >= 2)
                    {
                        return m_tracking->ShutdownAsync().
                            then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>&) noexcept
                            {
                                FailedOnce = true;
                                m_tracking = nullptr;

                                return InitSchedule();
                            });
                    }
                    else if (FailedOnce && m_tracking->Iterations >= 2)
                    {
                        return m_tracking->ShutdownAsync().
                            then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>&) noexcept
                        {
                            m_tracking.reset();

                            return Die();
                        });
                    }
                    else
                    {
                        return TrackingSchedule();
                    }
                });
            }

            arcana::task<void, std::error_code> InitSchedule()
            {
                if (!m_init)
                {
                    m_init = std::make_unique<InitializationWorker>(Context);
                }

                return arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    return Driver.move_to(TrackingInit, arcana::cancellation::none());
                }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const bool& initialized) noexcept
                {
                    if (!initialized)
                    {
                        return InitSchedule();
                    }
                    else
                    {
                        return m_init->ShutdownAsync()
                            .then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                            {
                                m_init.reset();

                                return TrackingSchedule();
                            });
                    }
                });
            }
        };

        TEST_METHOD(DynamicRuntime)
        {
            Runtime runtime;

            bool completed = false;

            runtime.Start().then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>& result) noexcept
            {
                Assert::IsTrue(result.error() == std::errc::owner_dead);
                completed = true;
            });

            do
            {
                if (!completed)
                {
                    runtime.Mediator.send<ImageReceived>({});
                }
            } while (runtime.Dispatcher.tick(arcana::cancellation::none()));
        }
    };
}
