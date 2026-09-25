#include "ARCoreInitialization.h"
#include <algorithm>
#include <exception>
#include <utility>

namespace arcore {
ARCoreInitialization::ARCoreInitialization(
    IPlatformServices& platform, IInitializationScheduler& scheduler,
    std::function<void(int, bool, const std::string&)> notifyViews)
    : platform_(platform), scheduler_(scheduler), notifyViews_(std::move(notifyViews)) {}

void ARCoreInitialization::registerRuntime(RuntimeId runtime) {
    scheduler_.assertMainThread();
    runtimes_.try_emplace(runtime, std::nullopt);
}

void ARCoreInitialization::removeRuntime(RuntimeId runtime) {
    scheduler_.assertMainThread();
    runtimes_.erase(runtime);
    std::vector<Waiter> removed;
    for (auto it = waiters_.begin(); it != waiters_.end();) {
        if (it->runtime == runtime) {
            removed.push_back(std::move(*it));
            it = waiters_.erase(it);
        } else ++it;
    }
    for (auto& waiter : removed)
        waiter.completion(false, "React runtime was destroyed during AR initialization");
    if (waiters_.empty() && viewTasks_.empty() &&
        (phase_ == Phase::Checking || phase_ == Phase::AwaitingHost))
        finish(false);
}

void ARCoreInitialization::initialize(RuntimeId runtime, InitializationCompletion completion) {
    scheduler_.assertMainThread();
    auto found = runtimes_.find(runtime);
    if (found == runtimes_.end()) {
        completion(false, "AR initialization runtime is unavailable");
        return;
    }
    if (pending()) {
        waiters_.push_back({runtime, std::move(completion)});
        return;
    }
    if (!found->second || !platform_.hasResumedHost(*found->second)) {
        completion(false, "AR initialization requires a resumed host Activity");
        return;
    }
    waiters_.push_back({runtime, std::move(completion)});
    begin(*found->second);
}

bool ARCoreInitialization::ensureForSession(int taskId) {
    scheduler_.assertMainThread();
    if (pending()) {
        viewTasks_.insert(taskId);
        return false;
    }
    if (!platform_.hasResumedHost(taskId)) return false;
    if (readyTasks_.count(taskId)) return true;
    if (attemptedTasks_.count(taskId)) return false;
    begin(taskId);
    if (pending()) viewTasks_.insert(taskId);
    return readyTasks_.count(taskId) != 0;
}

void ARCoreInitialization::hostResumed(RuntimeId runtime, int taskId) {
    scheduler_.assertMainThread();
    if (auto found = runtimes_.find(runtime); found != runtimes_.end())
        found->second = taskId;
    if (taskId != taskId_) return;
    if (phase_ == Phase::Installing && paused_) {
        phase_ = Phase::Continuing;
        install(false);
    } else if (phase_ == Phase::AwaitingHost) {
        phase_ = Phase::Checking;
        check();
    }
}

void ARCoreInitialization::hostPaused(int taskId) {
    scheduler_.assertMainThread();
    if (taskId == taskId_ && phase_ == Phase::Installing) paused_ = true;
}

void ARCoreInitialization::hostDestroyed(int taskId, bool changingConfigurations, bool finishing) {
    scheduler_.assertMainThread();
    if (taskId == taskId_ && pending()) {
        if (changingConfigurations) hostPaused(taskId);
        else if (finishing) finish(false);
    }
    if (finishing && !changingConfigurations) {
        readyTasks_.erase(taskId);
        attemptedTasks_.erase(taskId);
    }
}

bool ARCoreInitialization::pending() const {
    scheduler_.assertMainThread();
    return phase_ != Phase::Idle;
}

void ARCoreInitialization::begin(int taskId) {
    taskId_ = taskId;
    ++generation_;
    deadline_.reset();
    paused_ = false;
    attemptedTasks_.insert(taskId);
    readyTasks_.erase(taskId);
    phase_ = Phase::Checking;
    check();
}

void ARCoreInitialization::check() {
    try {
        if (deadline_ && scheduler_.now() >= *deadline_) {
            finish(false, "Timed out checking ARCore availability");
            return;
        }
        switch (platform_.checkAvailability()) {
        case AR_AVAILABILITY_UNKNOWN_CHECKING: {
            if (!deadline_) deadline_ = scheduler_.now() + 5000;
            const auto generation = generation_;
            timer_ = scheduler_.schedule(std::min<int64_t>(200, *deadline_ - scheduler_.now()), [this, generation] {
                if (generation != generation_ || phase_ != Phase::Checking) return;
                timer_ = 0;
                check();
            });
            return;
        }
        case AR_AVAILABILITY_UNSUPPORTED_DEVICE_NOT_CAPABLE:
            finish(false);
            return;
        case AR_AVAILABILITY_SUPPORTED_INSTALLED:
        case AR_AVAILABILITY_SUPPORTED_NOT_INSTALLED:
        case AR_AVAILABILITY_SUPPORTED_APK_TOO_OLD:
            cancelTimer();
            deadline_.reset();
            if (!platform_.hasResumedHost(taskId_)) {
                phase_ = Phase::AwaitingHost;
                return;
            }
            phase_ = Phase::Installing;
            install(true);
            return;
        case AR_AVAILABILITY_UNKNOWN_TIMED_OUT:
            finish(false, "Timed out checking ARCore availability");
            return;
        default:
            finish(false, "Could not determine ARCore availability");
        }
    } catch (const std::exception& error) {
        finish(false, error.what());
    }
}

void ARCoreInitialization::install(bool userRequested) {
    try {
        const auto result = platform_.requestInstall(taskId_, userRequested);
        if (result.status == AR_UNAVAILABLE_USER_DECLINED_INSTALLATION ||
            result.status == AR_UNAVAILABLE_DEVICE_NOT_COMPATIBLE) {
            finish(false);
        } else if (result.status != AR_SUCCESS) {
            finish(false, "ARCore installation failed (status " + std::to_string(result.status) + ")");
        } else if (result.installation == AR_INSTALL_STATUS_INSTALLED) {
            finish(true);
        } else if (!userRequested) {
            finish(false);
        } else if (result.installation != AR_INSTALL_STATUS_INSTALL_REQUESTED) {
            finish(false, "Unexpected ARCore installation status");
        }
    } catch (const std::exception& error) {
        finish(false, error.what());
    }
}

void ARCoreInitialization::cancelTimer() {
    if (timer_) scheduler_.cancel(timer_);
    timer_ = 0;
}

void ARCoreInitialization::finish(bool ready, const std::string& error) {
    cancelTimer();
    ++generation_;
    deadline_.reset();
    phase_ = Phase::Idle;
    paused_ = false;
    auto waiters = std::move(waiters_);
    waiters_.clear();
    auto views = std::move(viewTasks_);
    viewTasks_.clear();
    if (ready) {
        readyTasks_.insert(taskId_);
        readyTasks_.insert(views.begin(), views.end());
    }
    attemptedTasks_.insert(views.begin(), views.end());
    const auto generation = generation_;
    for (auto& waiter : waiters) waiter.completion(ready, error);
    for (int task : views) {
        scheduler_.dispatch([this, generation, task, ready, error] {
            if (generation == generation_) notifyViews_(task, ready, error);
        });
    }
}
}
