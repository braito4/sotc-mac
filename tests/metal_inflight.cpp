// Original CPU-only model of native submission admission and completion.
#include "gpu_metal_inflight.h"
#include <cassert>
#include <future>
#include <thread>
#include <vector>
extern "C" int FixtureStartControl();

int main(int argc, char** argv) {
    using namespace SotcMetalInflight;
    if (argc > 1 && std::strcmp(argv[1], "control") == 0) {
        if (FixtureStartControl()) return 98;
        Gate blocked(1);
        auto held = blocked.Acquire();
        std::thread observer([&] {
            while (blocked.Snapshot().waits == 0) std::this_thread::yield();
            std::puts("admission blocked; cancellation channel live");
            std::fflush(stdout);
        });
        auto next = blocked.Acquire();
        observer.join();
        return 99;
    }
    if (argc > 1) {
        std::printf("limit=%u\n", ConfiguredLimit());
        return 0;
    }
    Gate unlimited(0);
    std::vector<std::shared_ptr<Gate::Ticket>> pending;
    for (unsigned i = 0; i < 32; ++i) pending.push_back(unlimited.Acquire());
    assert(unlimited.Snapshot().peak == 32);
    for (auto& item : pending) item->Complete();
    assert(unlimited.Snapshot().active == 0 && unlimited.Snapshot().completed == 32);

    Gate bounded(8);
    pending.clear();
    for (unsigned i = 0; i < 8; ++i) pending.push_back(bounded.Acquire());
    std::promise<void> entered;
    auto entered_future = entered.get_future();
    auto next = std::async(std::launch::async, [&] {
        entered.set_value();
        return bounded.Acquire();
    });
    entered_future.get();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (bounded.Snapshot().waits == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    assert(bounded.Snapshot().waits == 1);
    assert(next.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout);
    // Out-of-order native completion, not a submitted/returned/timeline event.
    pending[5]->Complete();
    assert(next.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    auto ninth = next.get();
    assert(bounded.Snapshot().active == 8 && bounded.Snapshot().peak == 8);
    pending[5]->Complete(); // Duplicated callbacks cannot release a second slot.
    assert(bounded.Snapshot().active == 8 && bounded.Snapshot().completed == 1);
    for (auto& item : pending) item->Complete();
    ninth->Complete();
    assert(bounded.Snapshot().active == 0 && bounded.Snapshot().completed == 9);
    assert(bounded.Snapshot().waits == 1 && bounded.Snapshot().wait_ns > 0);
    {
        auto uncommitted = bounded.Acquire();
        auto callback_copy = uncommitted;
        uncommitted.reset();
        assert(bounded.Snapshot().active == 1);
        callback_copy.reset(); // An unsubmitted object is abandoned, not GPU progress.
    }
    assert(bounded.Snapshot().active == 0 && bounded.Snapshot().abandoned == 1);
    std::shared_ptr<Gate::Ticket> survivor;
    {
        Gate temporary(1);
        survivor = temporary.Acquire();
    }
    survivor->Complete(); // Callback state outlives the admitting Gate object.

    // Multiple recording producers, all using the same queue budget.
    Gate shared(3);
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < 6; ++i) threads.emplace_back([&] {
        for (unsigned j = 0; j < 100; ++j) {
            auto ticket = shared.Acquire();
            std::this_thread::yield();
            ticket->Complete();
        }
    });
    for (auto& thread : threads) thread.join();
    const auto stats = shared.Snapshot();
    assert(stats.active == 0 && stats.peak <= 3 && stats.completed == 600 && stats.abandoned == 0);
    std::puts("PASS: model peak 32 without admission / 8 with admission; callback-only "
              "release, duplicate/out-of-order/uncommitted/concurrent/lifetime controls; no GPU");
}
