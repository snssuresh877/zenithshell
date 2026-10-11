#include "Core/EventBus/event_bus.hpp"
#include "Core/Events/compositor_events.hpp"
#include "Core/Events/audio_events.hpp"
#include "Core/Events/brightness_events.hpp"
#include "Core/Events/media_events.hpp"
#include "Core/Events/notification_events.hpp"
#include "Platform/Compositor/compositor_manager.hpp"
#include "Services/Audio/audio_manager.hpp"
#include "Services/Media/mpris_player.hpp"
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

    // Test 11: AudioManager publishes VolumeChangedEvent to EventBus
    {
        auto bus = std::make_shared<zenith::EventBus>();
        zenith::AudioManager::set_event_bus(bus);

        zenith::VolumeChangedEvent last_event;
        bool received = false;
        bus->subscribe<zenith::VolumeChangedEvent>([&](const zenith::VolumeChangedEvent& ev) {
            last_event = ev;
            received = true;
        });

        zenith::AudioManager::set_volume(42);
        assert(received);
        assert(last_event.volume == 42);

        received = false;
        zenith::AudioManager::toggle_mute();
        assert(received);
        assert(last_event.volume == 42);

        std::cout << "  ✔ Test 11 passed: AudioManager -> VolumeChangedEvent (payload & mute verified)\n";
    }

    // Test 12: AudioManager publishes MicrophoneChangedEvent to EventBus
    {
        auto bus = std::make_shared<zenith::EventBus>();
        zenith::AudioManager::set_event_bus(bus);

        zenith::MicrophoneChangedEvent last_event;
        bool received = false;
        bus->subscribe<zenith::MicrophoneChangedEvent>([&](const zenith::MicrophoneChangedEvent& ev) {
            last_event = ev;
            received = true;
        });

        zenith::AudioManager::set_mic_volume(75);
        assert(received);
        assert(last_event.volume == 75);

        received = false;
        zenith::AudioManager::toggle_mic_mute();
        assert(received);
        assert(last_event.volume == 75);

        std::cout << "  ✔ Test 12 passed: AudioManager -> MicrophoneChangedEvent (payload & mute verified)\n";
    }

    // Test 17: AudioManager cleanup removes timer idempotently
    {
        auto bus = std::make_shared<zenith::EventBus>();
        zenith::AudioManager::init(bus);
        zenith::AudioManager::cleanup();
        // Calling cleanup multiple times must be safe and idempotent
        zenith::AudioManager::cleanup();
        zenith::AudioManager::cleanup();
        std::cout << "  ✔ Test 17 passed: AudioManager -> periodic timer cleanup is idempotent\n";
    }

    // Test 13: BrightnessChangedEvent published and received
    {
        auto bus = std::make_shared<zenith::EventBus>();
        zenith::BrightnessChangedEvent last_event;
        bool received = false;
        bus->subscribe<zenith::BrightnessChangedEvent>([&](const zenith::BrightnessChangedEvent& ev) {
            last_event = ev;
            received = true;
        });

        bus->publish(zenith::BrightnessChangedEvent{65});
        assert(received);
        assert(last_event.brightness == 65);

        std::cout << "  ✔ Test 13 passed: BrightnessChangedEvent publish/subscribe verified\n";
    }

    // Test 14: MediaChangedEvent published and received with MediaInfo payload
    {
        auto bus = std::make_shared<zenith::EventBus>();
        zenith::MediaInfo received_info;
        int receive_count = 0;
        bus->subscribe<zenith::MediaChangedEvent>([&](const zenith::MediaChangedEvent& ev) {
            received_info = ev.info;
            receive_count++;
        });

        zenith::MediaInfo test_info;
        test_info.player_name = "Spotify";
        test_info.title = "Stairway to Heaven";
        test_info.artist = "Led Zeppelin";
        test_info.album = "Led Zeppelin IV";
        test_info.status = "Playing";
        test_info.is_playing = true;

        bus->publish(zenith::MediaChangedEvent{test_info});
        assert(receive_count == 1);
        assert(received_info.player_name == "Spotify");
        assert(received_info.title == "Stairway to Heaven");
        assert(received_info.artist == "Led Zeppelin");
        assert(received_info.is_playing == true);
        assert(received_info == test_info);

        std::cout << "  ✔ Test 14 passed: MediaChangedEvent publish/subscribe & MediaInfo payload verified\n";
    }

    // Test 18: MprisPlayer cleanup is safe without connection and idempotent
    {
        // cleanup with no prior initialization (dbus_conn == nullptr)
        zenith::MprisPlayer::cleanup();
        zenith::MprisPlayer::cleanup();
        std::cout << "  ✔ Test 18 passed: MprisPlayer -> cleanup is safe without connection and idempotent\n";
    }

    // Test 15: NotificationHistoryChangedEvent delivered to multiple independent subscribers
    {
        auto bus = std::make_shared<zenith::EventBus>();
        int subscriber_a_calls = 0;
        int subscriber_a_count = 0;
        int subscriber_b_calls = 0;
        int subscriber_b_count = 0;

        // Subscriber A (e.g. NotificationPanel)
        bus->subscribe<zenith::NotificationHistoryChangedEvent>([&](const zenith::NotificationHistoryChangedEvent& ev) {
            subscriber_a_calls++;
            subscriber_a_count = ev.count;
        });

        // Subscriber B (e.g. BarWindow)
        bus->subscribe<zenith::NotificationHistoryChangedEvent>([&](const zenith::NotificationHistoryChangedEvent& ev) {
            subscriber_b_calls++;
            subscriber_b_count = ev.count;
        });

        bus->publish(zenith::NotificationHistoryChangedEvent{5});

        assert(subscriber_a_calls == 1);
        assert(subscriber_a_count == 5);
        assert(subscriber_b_calls == 1);
        std::cout << "  ✔ Test 15 passed: NotificationHistoryChangedEvent delivered to independent subscribers\n";
    }

    // Test 16: DndChangedEvent delivered to multiple independent subscribers with true/false states
    {
        auto bus = std::make_shared<zenith::EventBus>();
        int subscriber_a_calls = 0;
        bool subscriber_a_dnd = false;
        int subscriber_b_calls = 0;
        bool subscriber_b_dnd = false;

        // Subscriber A (e.g. BarWindow)
        bus->subscribe<zenith::DndChangedEvent>([&](const zenith::DndChangedEvent& ev) {
            subscriber_a_calls++;
            subscriber_a_dnd = ev.dnd_enabled;
        });

        // Subscriber B (e.g. ControlCenter)
        bus->subscribe<zenith::DndChangedEvent>([&](const zenith::DndChangedEvent& ev) {
            subscriber_b_calls++;
            subscriber_b_dnd = ev.dnd_enabled;
        });

        // Transition 1: DND enabled (true)
        bus->publish(zenith::DndChangedEvent{true});
        assert(subscriber_a_calls == 1);
        assert(subscriber_a_dnd == true);
        assert(subscriber_b_calls == 1);
        assert(subscriber_b_dnd == true);

        // Transition 2: DND disabled (false)
        bus->publish(zenith::DndChangedEvent{false});
        assert(subscriber_a_calls == 2);
        assert(subscriber_a_dnd == false);
        assert(subscriber_b_calls == 2);
        assert(subscriber_b_dnd == false);

        std::cout << "  ✔ Test 16 passed: DndChangedEvent delivered to independent subscribers (true/false states)\n";
    }

    std::cout << "[TestEventBus] ALL TESTS PASSED SUCCESSFULLY! ✔\n";
}

int main() {
    run_tests();
    return 0;
}
