// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace slayer {
namespace fs = std::filesystem;
#ifndef SLAYERCTL_VERSION
#define SLAYERCTL_VERSION "development"
#endif
inline constexpr const char *version = SLAYERCTL_VERSION;
struct Error : std::runtime_error {
    int code;
    explicit Error(std::string message, int status = 2) : std::runtime_error(std::move(message)), code(status) {}
};
struct Options { fs::path root = "/"; bool dry = false; bool tsv = false; };
struct Led { std::string path; int maximum; int index; std::vector<int> channelMax; std::vector<std::string> channels; };
class Io {
    Options options;
    int testFailure=0;
    int testPostFailure=0;
    mutable int testWrites=0;
    // Descriptor snapshot for this command only. Live colors/brightness and
    // all write readbacks bypass this cache; no background invalidation work.
    mutable std::optional<std::vector<Led>> ledCache;
    mutable std::optional<std::string> compatibilityCache;
    friend const std::vector<Led>& leds(const Io&);
public:
    explicit Io(Options opts);
    bool fixture() const;
    std::string compatibilityIssue() const;
    void requireCompatible() const;
    bool dry() const { return options.dry; }
    fs::path path(const std::string &absolute) const;
    bool exists(const std::string &absolute) const;
    std::string read(const std::string &absolute, std::size_t limit = 4096) const;
    std::vector<std::string> list(const std::string &absolute) const;
    void write(const std::string &absolute, const std::string &value) const;
};
// Process-scoped serialization; kernel/external tools retain their own locks.
class FileLock {
    int fd = -1;
public:
    FileLock(const Io&, const std::string& name);
    ~FileLock();
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
};
struct Capability { std::string id, name, state, reason; };
std::string trim(std::string value);
std::string displayText(std::string value);
std::vector<std::string> words(const std::string &value);
int integer(const std::string &text, int low, int high, const std::string &name);
std::string capture(const std::vector<std::string> &argv, int timeoutMs = 3000);
const std::vector<Led>& leds(const Io &io);
std::vector<std::string> batteries(const Io &io);
std::vector<Capability> capabilities(const Io &io);
std::map<std::string, std::string> status(const Io &io);
void keyboard(const Io &io, const std::vector<std::string> &args);
void battery(const Io &io, const std::vector<std::string> &args);
void usb(const Io &io, const std::vector<std::string> &args);
void profile(const Io &io, const std::vector<std::string> &args);
void gpu(const Io &io, const std::vector<std::string> &args);
void fan(const Io &io, const std::vector<std::string> &args);
void sensors(const Io &io);
void telemetry(const Io &io);
void firmware(const Io &io);
void doctor(const Io &io);
void effect(const Io &io, const std::vector<std::string> &args);
std::string capabilityReason(const Io &io, const std::string &id);
[[noreturn]] void unverified(const std::string &feature);
}
