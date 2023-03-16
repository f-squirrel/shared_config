#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include <cassert>
#include <cstdint>
#include <sys/types.h>

class NonCopiableNonMovable {
public:
    NonCopiableNonMovable()                              = default;
    NonCopiableNonMovable(const NonCopiableNonMovable &) = delete;
    NonCopiableNonMovable(NonCopiableNonMovable &&)      = delete;

    NonCopiableNonMovable &operator=(const NonCopiableNonMovable &) = delete;
    NonCopiableNonMovable &operator=(NonCopiableNonMovable &&)      = delete;

    ~NonCopiableNonMovable() = default;
};

class InnerConfig /*: private NonCopiableNonMovable*/ {
public:
    InnerConfig(const std::string &name, u_int16_t version)
        : name_{name}, version_{version} {}

    // InnerConfig(const InnerConfig &) = delete;
    // InnerConfig& operator = (const InnerConfig &) = delete;

    const std::string &name() const { return name_; }
    uint16_t version() const { return version_; }

private:
    std::string name_;
    uint16_t version_;
};

class Config : private NonCopiableNonMovable {
public:
    Config(std::weak_ptr<const InnerConfig> inner_config)
        : inner_config_{inner_config} {}

    const std::string &name() const { return lock()->name(); }
    uint16_t version() const { return lock()->version(); }

private:
    std::shared_ptr<const InnerConfig> lock() const {
        auto sp = inner_config_.lock();
        assert(sp);
        return sp;
    }

private:
    std::weak_ptr<const InnerConfig> inner_config_;
};

class ConfigManager : private NonCopiableNonMovable {
public:
    ConfigManager(const std::string config_path,
                  std::chrono::milliseconds reload_interval)
        : config_path_{config_path}, reload_interval_{reload_interval} {

        last_used_    = read();
        inner_config_ = std::make_shared<InnerConfig>(parse(last_used_));
        stop_         = false;
        watcher_      = std::thread([&]() { watch(); });
    }

    ~ConfigManager() {
        stop_ = true;
        watcher_.join();
    }

    ConfigManager(const ConfigManager &) = delete;
    ConfigManager(ConfigManager &&)      = delete;

    ConfigManager &operator=(const ConfigManager &) = delete;
    ConfigManager &operator=(ConfigManager &&)      = delete;

    Config get() const { return Config{inner_config_}; }

private:
    void watch() {
        for (; !stop_; std::this_thread::sleep_for(reload_interval_)) {
            auto latest = read();
            if (latest != last_used_) {
                inner_config_ = std::make_shared<InnerConfig>(parse(latest));
                last_used_    = std::move(latest);
            }
        }
    }

    std::string read() const {
        auto file   = std::ifstream{config_path_.c_str()};
        auto latest = std::string{
            (std::istreambuf_iterator<std::string::value_type>(file)),
            std::istreambuf_iterator<std::string::value_type>()};
        file.close();
        return latest;
    }

    InnerConfig parse(const std::string &) const {
        // TODO: implement deserialization from string
        return InnerConfig{"name", 42};
    }

private:
    const std::string config_path_;
    const std::chrono::milliseconds reload_interval_;

    std::shared_ptr<const InnerConfig> inner_config_;
    std::string last_used_;
    std::atomic_bool stop_ = true;
    std::thread watcher_;
};
