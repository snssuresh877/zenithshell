#include "Core/EventBus/event_bus.hpp"
#include "Core/Events/compositor_events.hpp"
#include "Platform/Compositor/compositor_manager.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

struct TestEventA {
    int value = 0;
};

struct TestEventB {
    std::string message;
};

void run_tests() {
    std::cout << "[TestEventBus] Running unit test suite...\n";

    // Test 1: subscribe, publish, callback executes
    {
        zenith::EventBus bus;
        int received_val = 0;
        zenith::SubscriptionId id = bus.subscribe<TestEventA>([&](const TestEventA& e) {
            received_val = e.value;
        });
        assert(id != zenith::INVALID_SUBSCRIPTION_ID);
        assert(bus.subscriber_count<TestEventA>() == 1);

        bus.publish(TestEventA{42});
        assert(received_val == 42);
        std::cout << "  ✔ Test 1 passed: subscribe and publish\n";
    }

    // Test 2: subscribe, publish, unsubscribe, publish -> no execution
    {
        zenith::EventBus bus;
        int call_count = 0;
        zenith::SubscriptionId id = bus.subscribe<TestEventA>([&](const TestEventA&) {
            call_count++;
        });

        bus.publish(TestEventA{1});
        assert(call_count == 1);

        bool unsub_ok = bus.unsubscribe<TestEventA>(id);
        assert(unsub_ok);
        assert(bus.subscriber_count<TestEventA>() == 0);

        bus.publish(TestEventA{2});
        assert(call_count == 1);
        std::cout << "  ✔ Test 2 passed: unsubscribe stops reception\n";
    }

    // Test 3: callback unsubscribes itself -> no iterator corruption
    {
        zenith::EventBus bus;
        int call_count = 0;
        zenith::SubscriptionId self_id = zenith::INVALID_SUBSCRIPTION_ID;
        self_id = bus.subscribe<TestEventA>([&](const TestEventA&) {
            call_count++;
            bus.unsubscribe<TestEventA>(self_id);
        });

        bus.publish(TestEventA{10});
        assert(call_count == 1);
        assert(bus.subscriber_count<TestEventA>() == 0);

        bus.publish(TestEventA{20});
        assert(call_count == 1);
        std::cout << "  ✔ Test 3 passed: self-unsubscribe during dispatch\n";
    }

    // Test 4: callback A unsubscribes callback B during dispatch -> valid dispatch, B does not execute
    {
        zenith::EventBus bus;
        bool b_called = false;
        zenith::SubscriptionId idB = zenith::INVALID_SUBSCRIPTION_ID;

        bus.subscribe<TestEventA>([&](const TestEventA&) {
            bus.unsubscribe<TestEventA>(idB);
        });

        idB = bus.subscribe<TestEventA>([&](const TestEventA&) {
            b_called = true;
        });

        assert(bus.subscriber_count<TestEventA>() == 2);
        bus.publish(TestEventA{99});
        assert(!b_called);
        assert(bus.subscriber_count<TestEventA>() == 1);
        std::cout << "  ✔ Test 4 passed: cross-unsubscribe during dispatch\n";
    }

    // Test 5: multiple subscribers -> all execute
    {
        zenith::EventBus bus;
        int sum = 0;
        bus.subscribe<TestEventA>([&](const TestEventA& e) { sum += e.value; });
        bus.subscribe<TestEventA>([&](const TestEventA& e) { sum += e.value * 2; });
        bus.subscribe<TestEventA>([&](const TestEventA& e) { sum += e.value * 3; });

        assert(bus.subscriber_count<TestEventA>() == 3);
        bus.publish(TestEventA{5}); // 5 + 10 + 15 = 30
        assert(sum == 30);
        std::cout << "  ✔ Test 5 passed: multiple subscribers\n";
    }

    // Test 6: different event types -> isolated dispatch
    {
        zenith::EventBus bus;
        int a_count = 0;
        int b_count = 0;

        bus.subscribe<TestEventA>([&](const TestEventA&) { a_count++; });
        bus.subscribe<TestEventB>([&](const TestEventB&) { b_count++; });

        bus.publish(TestEventA{1});
        assert(a_count == 1);
        assert(b_count == 0);

        bus.publish(TestEventB{"hello"});
        assert(a_count == 1);
        assert(b_count == 1);
        std::cout << "  ✔ Test 6 passed: typed event isolation\n";
    }

    // Test 7: reentrant publishing
    {
        zenith::EventBus bus;
        std::vector<std::string> log;

        bus.subscribe<TestEventA>([&](const TestEventA& e) {
            log.push_back("A_start:" + std::to_string(e.value));
            bus.publish(TestEventB{"nested"});
            log.push_back("A_end:" + std::to_string(e.value));
        });

        bus.subscribe<TestEventB>([&](const TestEventB& e) {
            log.push_back("B:" + e.message);
        });

        bus.publish(TestEventA{7});

        assert(log.size() == 3);
        assert(log[0] == "A_start:7");
        assert(log[1] == "B:nested");
        assert(log[2] == "A_end:7");
        std::cout << "  ✔ Test 7 passed: reentrant dispatch\n";
    }

    // Test 8: CompositorManager -> WorkspaceChangedEvent -> EventBus subscriber receives event
    {
        auto bus = std::make_shared<zenith::EventBus>();
        zenith::CompositorManager::instance().set_event_bus(bus);

        int received_ws = -1;
        bus->subscribe<zenith::WorkspaceChangedEvent>([&](const zenith::WorkspaceChangedEvent& ev) {
            received_ws = ev.workspace_id;
        });

        zenith::CompositorManager::instance().notify_workspace(4);
        assert(received_ws == 4);
        std::cout << "  ✔ Test 8 passed: CompositorManager -> WorkspaceChangedEvent\n";
    }

    // Test 9: CompositorManager -> WindowTitleChangedEvent -> EventBus subscriber receives event
    {
        auto bus = std::make_shared<zenith::EventBus>();
        zenith::CompositorManager::instance().set_event_bus(bus);

        std::string received_title;
        bus->subscribe<zenith::WindowTitleChangedEvent>([&](const zenith::WindowTitleChangedEvent& ev) {
            received_title = ev.title;
        });

        zenith::CompositorManager::instance().notify_window_title("Zenith Development");
        assert(received_title == "Zenith Development");
        std::cout << "  ✔ Test 9 passed: CompositorManager -> WindowTitleChangedEvent\n";
    }

    // Test 10: CompositorManager::notify_window_event() publishes WindowListChangedEvent to EventBus
    {
        auto bus = std::make_shared<zenith::EventBus>();
        zenith::CompositorManager::instance().set_event_bus(bus);

        bool event_received = false;
        bus->subscribe<zenith::WindowListChangedEvent>([&](const zenith::WindowListChangedEvent&) {
            event_received = true;
        });

        zenith::CompositorManager::instance().notify_window_event();
        assert(event_received);
        std::cout << "  ✔ Test 10 passed: CompositorManager -> WindowListChangedEvent\n";
    }

    std::cout << "[TestEventBus] ALL TESTS PASSED SUCCESSFULLY! ✔\n";
}

int main() {
    run_tests();
    return 0;
}
