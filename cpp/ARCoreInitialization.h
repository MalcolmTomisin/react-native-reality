#pragma once

#include "IPlatformServices.h"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace arcore {
using RuntimeId = int64_t;
using InitializationCompletion = std::function<void(bool, const std::string&)>;

class IInitializationScheduler {
public:
    using Token = uint64_t;
    virtual ~IInitializationScheduler() = default;
    virtual void assertMainThread() const = 0;
    virtual void dispatch(std::function<void()> action) = 0;
    virtual Token schedule(int64_t delayMs, std::function<void()> action) = 0;
    virtual void cancel(Token token) = 0;
    virtual int64_t now() const = 0;
};

// All state and callbacks are confined to the scheduler's main thread.
class ARCoreInitialization {
public:
    ARCoreInitialization(IPlatformServices& platform, IInitializationScheduler& scheduler,
                         std::function<void(int, bool, const std::string&)> notifyViews);
    void registerRuntime(RuntimeId runtime);
    void removeRuntime(RuntimeId runtime);
    void initialize(RuntimeId runtime, InitializationCompletion completion);
    bool ensureForSession(int taskId);
    void hostResumed(RuntimeId runtime, int taskId);
    void hostPaused(int taskId);
    void hostDestroyed(int taskId, bool changingConfigurations, bool finishing);
    bool pending() const;

private:
    enum class Phase { Idle, Checking, AwaitingHost, Installing, Continuing };
    struct Waiter { RuntimeId runtime; InitializationCompletion completion; };
    void begin(int taskId);
    void check();
    void install(bool userRequested);
    void finish(bool ready, const std::string& error = {});
    void cancelTimer();

    IPlatformServices& platform_;
    IInitializationScheduler& scheduler_;
    std::function<void(int, bool, const std::string&)> notifyViews_;
    std::map<RuntimeId, std::optional<int>> runtimes_;
    std::vector<Waiter> waiters_;
    std::set<int> viewTasks_;
    std::set<int> attemptedTasks_;
    std::set<int> readyTasks_;
    Phase phase_ = Phase::Idle;
    int taskId_ = -1;
    bool paused_ = false;
    uint64_t generation_ = 0;
    IInitializationScheduler::Token timer_ = 0;
    std::optional<int64_t> deadline_;
};
}
