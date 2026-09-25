#include "ARCoreInitialization.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <tuple>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
using namespace arcore;

struct Scheduler : IInitializationScheduler {
    struct Task { int64_t at; std::function<void()> action; };
    int64_t time = 0;
    Token next = 0;
    bool main = true;
    std::map<Token, Task> tasks;
    void assertMainThread() const override { if (!main) throw std::runtime_error("not main"); }
    void dispatch(std::function<void()> action) override { schedule(0, std::move(action)); }
    Token schedule(int64_t delay, std::function<void()> action) override {
        auto token = ++next;
        tasks.emplace(token, Task{time + delay, std::move(action)});
        return token;
    }
    void cancel(Token token) override { tasks.erase(token); }
    int64_t now() const override { return time; }
    void tick() {
        CHECK(!tasks.empty());
        auto it = std::min_element(tasks.begin(), tasks.end(), [](auto& a, auto& b) { return a.second.at < b.second.at; });
        auto task = std::move(it->second);
        tasks.erase(it);
        time = std::max(time, task.at);
        task.action();
    }
};

struct Platform : IPlatformServices {
    struct Host { int activity; bool resumed; };
    std::map<int, Host> hosts;
    ArAvailability availability = AR_AVAILABILITY_SUPPORTED_INSTALLED;
    ARCoreInstallResult result{AR_SUCCESS, AR_INSTALL_STATUS_INSTALLED};
    std::vector<std::tuple<int, int, bool>> installs;
    int checks = 0;
    bool fail = false;
    ArAvailability checkAvailability() override {
        ++checks;
        if (fail) throw std::runtime_error("native failure");
        return availability;
    }
    ARCoreInstallResult requestInstall(int task, bool requested) override {
        CHECK(hasResumedHost(task));
        installs.emplace_back(task, hosts.at(task).activity, requested);
        return result;
    }
    bool hasResumedHost(int task) override { return hosts.count(task) && hosts.at(task).resumed; }
    bool isGooglePlayServicesAvailable() override { return false; }
};

struct Harness {
    Scheduler scheduler;
    Platform platform;
    struct Result { int caller; bool ready; std::string error; };
    std::vector<Result> results;
    std::vector<Result> views;
    ARCoreInitialization coordinator{platform, scheduler, [this](int task, bool ready, const std::string& error) {
        views.push_back({task, ready, error});
    }};
    Harness() {
        coordinator.registerRuntime(1);
        coordinator.registerRuntime(2);
        resume(1, 10, 100);
    }
    void start(int caller = 1, RuntimeId runtime = 1) {
        coordinator.initialize(runtime, [this, caller](bool ready, const std::string& error) {
            results.push_back({caller, ready, error});
        });
    }
    void resume(RuntimeId runtime = 1, int task = 10, int activity = 100) {
        platform.hosts[task] = {activity, true};
        coordinator.hostResumed(runtime, task);
    }
    void pause(int task = 10) {
        platform.hosts.at(task).resumed = false;
        coordinator.hostPaused(task);
    }
    void destroy(bool changing = true, bool finishing = false) {
        platform.hosts.erase(10);
        coordinator.hostDestroyed(10, changing, finishing);
    }
    void installing() { platform.result = {AR_SUCCESS, AR_INSTALL_STATUS_INSTALL_REQUESTED}; }
    void installed() { platform.result = {AR_SUCCESS, AR_INSTALL_STATUS_INSTALLED}; }
};

void installedAndUnsupported() {
    Harness h;
    h.start();
    CHECK(h.results.size() == 1 && h.results[0].ready);
    CHECK(h.platform.installs.size() == 1 && std::get<2>(h.platform.installs[0]));
    h.platform.availability = AR_AVAILABILITY_UNSUPPORTED_DEVICE_NOT_CAPABLE;
    h.start(2);
    CHECK(!h.results[1].ready && h.results[1].error.empty());
    CHECK(h.platform.installs.size() == 1);
}
void missingAndOutdated() {
    for (auto availability : {AR_AVAILABILITY_SUPPORTED_NOT_INSTALLED, AR_AVAILABILITY_SUPPORTED_APK_TOO_OLD}) {
        Harness h;
        h.platform.availability = availability;
        h.installing();
        h.start();
        CHECK(h.coordinator.pending() && h.results.empty());
        h.pause();
        h.installed();
        h.resume();
        CHECK(h.results.size() == 1 && h.results[0].ready);
        CHECK(!std::get<2>(h.platform.installs[1]));
    }
}
void pendingAvailabilityAndDeadline() {
    Harness h;
    h.platform.availability = AR_AVAILABILITY_UNKNOWN_CHECKING;
    h.start();
    h.scheduler.tick();
    CHECK(h.scheduler.time == 200 && h.results.empty());
    h.platform.availability = AR_AVAILABILITY_SUPPORTED_INSTALLED;
    h.scheduler.tick();
    CHECK(h.results[0].ready);
    h.platform.availability = AR_AVAILABILITY_UNKNOWN_CHECKING;
    h.start(2);
    while (!h.scheduler.tasks.empty()) h.scheduler.tick();
    CHECK(h.scheduler.time == 5400);
    CHECK(h.results[1].error.find("Timed out") != std::string::npos);
}
void installationHasNoDeadline() {
    Harness h;
    h.platform.availability = AR_AVAILABILITY_UNKNOWN_CHECKING;
    h.start();
    auto stale = h.scheduler.tasks.begin()->second.action;
    h.platform.availability = AR_AVAILABILITY_SUPPORTED_INSTALLED;
    h.installing();
    h.scheduler.tick();
    h.pause();
    h.scheduler.time = 600000;
    stale();
    CHECK(h.scheduler.tasks.empty() && h.results.empty());
    h.installed();
    h.resume();
    CHECK(h.results[0].ready);
}
void cancellationIncompleteAndErrors() {
    for (auto status : {AR_UNAVAILABLE_USER_DECLINED_INSTALLATION, AR_UNAVAILABLE_DEVICE_NOT_COMPATIBLE, AR_SUCCESS, AR_ERROR_FATAL}) {
        Harness h;
        h.installing();
        h.start();
        h.pause();
        h.platform.result = {status, AR_INSTALL_STATUS_INSTALL_REQUESTED};
        h.resume();
        CHECK(h.results.size() == 1 && !h.results[0].ready);
        CHECK(h.results[0].error.empty() == (status != AR_ERROR_FATAL));
        h.resume();
        CHECK(h.platform.installs.size() == 2 && !h.coordinator.pending());
    }
}
void statusIsIgnoredOnFailure() {
    Harness h;
    h.platform.result = {AR_ERROR_FATAL, AR_INSTALL_STATUS_INSTALLED};
    h.start();
    CHECK(!h.results[0].ready && !h.results[0].error.empty());
}
void duplicateResumesAndUnrelatedTasks() {
    Harness h;
    h.installing();
    h.start();
    h.resume();
    CHECK(h.platform.installs.size() == 1);
    h.pause();
    h.installed();
    h.resume(2, 20, 200);
    CHECK(h.results.empty());
    h.resume();
    h.resume(0);
    CHECK(h.results.size() == 1 && h.platform.installs.size() == 2);
}
void replacementHostContinues() {
    Harness h;
    h.installing();
    h.start();
    h.pause();
    h.destroy();
    CHECK(h.results.empty() && h.coordinator.pending());
    h.installed();
    h.resume(1, 10, 101);
    CHECK(h.results[0].ready && std::get<1>(h.platform.installs[1]) == 101);
}
void finishingHostStopsOperation() {
    Harness h;
    h.installing();
    h.start();
    h.pause();
    h.destroy(false, true);
    CHECK(h.results.size() == 1 && !h.results[0].ready && h.results[0].error.empty());
    CHECK(!h.coordinator.pending());
}
void freshCallNeedsActivityButJoinDoesNot() {
    Harness h;
    h.start(1, 2);
    CHECK(!h.results[0].error.empty());
    h.installing();
    h.start(2);
    h.pause();
    h.start(3, 2);
    CHECK(h.results.size() == 1);
    h.installed();
    h.resume();
    CHECK(h.results.size() == 3 && h.results[1].ready && h.results[2].ready);
}
void runtimeTeardownIsIsolated() {
    Harness h;
    h.installing();
    h.start(1);
    h.start(2, 2);
    h.coordinator.removeRuntime(1);
    CHECK(h.results.size() == 1 && h.results[0].caller == 1 && !h.results[0].error.empty());
    h.pause();
    h.installed();
    h.resume(2, 10, 101);
    CHECK(h.results.size() == 2 && h.results[1].caller == 2 && h.results[1].ready);
}
void teardownRetainsInstallationGuard() {
    Harness h;
    h.installing();
    h.start();
    h.pause();
    h.coordinator.removeRuntime(1);
    CHECK(h.coordinator.pending());
    h.start(2, 2);
    CHECK(h.platform.installs.size() == 1);
    h.installed();
    h.resume(2, 10, 101);
    CHECK(h.results[1].ready && !std::get<2>(h.platform.installs[1]));
}
void teardownCancelsAvailabilityCallbacks() {
    Harness h;
    h.platform.availability = AR_AVAILABILITY_UNKNOWN_CHECKING;
    h.start();
    auto stale = h.scheduler.tasks.begin()->second.action;
    h.coordinator.removeRuntime(1);
    CHECK(h.scheduler.tasks.empty() && !h.coordinator.pending());
    stale();
    CHECK(h.platform.checks == 1 && h.results.size() == 1);
    h.start(2);
    CHECK(!h.results[1].error.empty());
}
void registrationIsIdempotent() {
    Harness h;
    h.coordinator.registerRuntime(1);
    h.start();
    CHECK(h.results[0].ready);
}
void viewsJoinAndRetryOnlyOnSuccess() {
    Harness h;
    h.installing();
    h.start();
    CHECK(!h.coordinator.ensureForSession(10));
    CHECK(!h.coordinator.ensureForSession(10));
    CHECK(h.platform.installs.size() == 1);
    h.pause();
    h.installed();
    h.resume();
    h.scheduler.tick();
    CHECK(h.views.size() == 1 && h.views[0].ready);
    CHECK(h.coordinator.ensureForSession(10));
    CHECK(h.platform.installs.size() == 2);
}
void viewStartedInstallationCanBeJoined() {
    Harness h;
    h.installing();
    CHECK(!h.coordinator.ensureForSession(10));
    h.start();
    CHECK(h.platform.installs.size() == 1);
    h.pause();
    h.installed();
    h.resume();
    h.scheduler.tick();
    CHECK(h.results[0].ready && h.views[0].ready);
}
void passiveFailureDoesNotReprompt() {
    Harness h;
    h.installing();
    CHECK(!h.coordinator.ensureForSession(10));
    h.pause();
    h.platform.result.status = AR_UNAVAILABLE_USER_DECLINED_INSTALLATION;
    h.resume();
    CHECK(!h.coordinator.ensureForSession(10));
    h.scheduler.tick();
    CHECK(h.platform.installs.size() == 2 && !h.views[0].ready);
    h.installed();
    h.start();
    CHECK(h.results[0].ready && std::get<2>(h.platform.installs[2]));
}
void availabilityErrorRejects() {
    for (auto value : {AR_AVAILABILITY_UNKNOWN_TIMED_OUT, AR_AVAILABILITY_UNKNOWN_ERROR}) {
        Harness h;
        h.platform.availability = value;
        h.start();
        CHECK(!h.results[0].error.empty());
    }
    Harness h;
    h.platform.fail = true;
    h.start();
    CHECK(h.results[0].error == "native failure");
}
void hostCanDisappearDuringAvailability() {
    Harness h;
    h.platform.availability = AR_AVAILABILITY_UNKNOWN_CHECKING;
    h.start();
    h.pause();
    h.destroy();
    h.platform.availability = AR_AVAILABILITY_SUPPORTED_INSTALLED;
    h.scheduler.tick();
    CHECK(h.results.empty() && h.platform.installs.empty());
    h.resume(1, 10, 101);
    CHECK(h.results[0].ready);
}
void staleViewCompletionCannotAffectNewAttempt() {
    Harness h;
    h.installing();
    h.coordinator.ensureForSession(10);
    h.pause();
    h.platform.result.status = AR_UNAVAILABLE_USER_DECLINED_INSTALLATION;
    h.resume();
    h.installing();
    h.start();
    h.scheduler.tick();
    CHECK(h.views.empty() && h.results.empty() && h.coordinator.pending());
}
void checkingContinuesForSurvivingRuntime() {
    Harness h;
    h.platform.availability = AR_AVAILABILITY_UNKNOWN_CHECKING;
    h.start();
    h.start(2, 2);
    h.coordinator.removeRuntime(1);
    h.platform.availability = AR_AVAILABILITY_SUPPORTED_INSTALLED;
    h.scheduler.tick();
    CHECK(h.results.size() == 2 && h.results[1].caller == 2 && h.results[1].ready);
}
void mainThreadIsRequired() {
    Harness h;
    h.scheduler.main = false;
    bool rejected = false;
    try { h.start(); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}

int main() {
    const std::vector<std::pair<const char*, void(*)()>> tests = {
        {"installed/unsupported", installedAndUnsupported}, {"missing/outdated", missingAndOutdated},
        {"availability deadline", pendingAvailabilityAndDeadline}, {"unlimited installation wait", installationHasNoDeadline},
        {"cancellation/incomplete/errors", cancellationIncompleteAndErrors}, {"invalid install status", statusIsIgnoredOnFailure},
        {"duplicate/unrelated resume", duplicateResumesAndUnrelatedTasks}, {"replacement Activity", replacementHostContinues},
        {"finishing task", finishingHostStopsOperation}, {"missing Activity/join", freshCallNeedsActivityButJoinDoesNot},
        {"runtime isolation", runtimeTeardownIsIsolated}, {"installation guard", teardownRetainsInstallationGuard},
        {"stale callbacks", teardownCancelsAvailabilityCallbacks}, {"registration", registrationIsIdempotent},
        {"view joins", viewsJoinAndRetryOnlyOnSuccess}, {"JS joins view", viewStartedInstallationCanBeJoined},
        {"passive failure/explicit retry", passiveFailureDoesNotReprompt}, {"availability errors", availabilityErrorRejects},
        {"host disappears during check", hostCanDisappearDuringAvailability}, {"main-thread confinement", mainThreadIsRequired},
        {"stale view notification", staleViewCompletionCannotAffectNewAttempt},
        {"surviving runtime during check", checkingContinuesForSurvivingRuntime},
    };
    for (const auto& [name, test] : tests) {
        try { test(); } catch (const std::exception& error) {
            std::cerr << name << ": " << error.what() << '\n';
            return 1;
        }
    }
    std::cout << tests.size() << " ARCore initialization tests passed\n";
}
