#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <cassert>
#include <cstdint>
#include <sys/types.h>

class NonCopyable {
public:
    NonCopyable()                          = default;
    NonCopyable(NonCopyable &&)            = default;
    NonCopyable &operator=(NonCopyable &&) = default;

    NonCopyable(const NonCopyable &)            = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;

    ~NonCopyable() = default;
};

class InnerConfig : private NonCopyable {
public:
    InnerConfig(const std::string &name, u_int16_t version)
        : name_{name}, version_{version} {}

    [[nodiscard]] const std::string &name() const { return name_; }
    [[nodiscard]] uint16_t version() const { return version_; }

private:
    std::string name_;
    uint16_t version_;
};

class Config {
public:
    Config(const std::shared_ptr<const InnerConfig> &inner_config)
        : inner_config_{inner_config} {}

    [[nodiscard]] std::string name() const { return load()->name(); }
    [[nodiscard]] uint16_t version() const { return load()->version(); }

private:
    [[nodiscard]] const std::shared_ptr<const InnerConfig> load() const {
        auto ptr = std::atomic_load(&inner_config_);
        assert(ptr);
        return ptr;
    }

    const std::shared_ptr<const InnerConfig> &inner_config_;
};

class ConfigManager : private NonCopyable {
public:
    ConfigManager(const std::string config_path,
                  std::chrono::milliseconds reload_interval)
        : config_path_{config_path}, reload_interval_{reload_interval} {

        last_used_   = std::move(read().value());
        auto new_ptr = std::make_shared<const InnerConfig>(
            std::move(parse(last_used_).value()));
        std::atomic_store(&inner_config_, new_ptr);

        stop_    = false;
        watcher_ = std::thread([&]() { watch(); });
    }

    ~ConfigManager() {
        stop_ = true;
        watcher_.join();
    }

    ConfigManager(ConfigManager &&)            = delete;
    ConfigManager &operator=(ConfigManager &&) = delete;

    Config get() const { return Config{inner_config_}; }

private:
    void watch() {
        for (; !stop_; std::this_thread::sleep_for(reload_interval_)) {
            auto latest = read();
            if (latest != last_used_) {
                std::cout << "reloaded\n";
                auto inner_config = parse(latest.value());
                if (!inner_config.has_value()) {
                    continue;
                }
                auto new_ptr = std::make_shared<const InnerConfig>(
                    std::move(inner_config.value()));
                std::atomic_store(&inner_config_, new_ptr);
                last_used_ = std::move(latest.value());
            }
        }
    }

    [[nodiscard]] std::optional<std::string> read() const {
        auto file = std::ifstream{config_path_.c_str()};
        if (!file.is_open()) {
            return {};
        }
        auto latest = std::string{
            (std::istreambuf_iterator<std::string::value_type>(file)),
            std::istreambuf_iterator<std::string::value_type>()};
        file.close();
        return {latest};
    }

    [[nodiscard]] std::optional<InnerConfig> parse(const std::string &) const {
        // TODO: implement deserialization from string
        return {InnerConfig{"name", 42}};
    }

private:
    const std::string config_path_;
    const std::chrono::milliseconds reload_interval_;

    std::shared_ptr<const InnerConfig> inner_config_;
    std::string last_used_;
    std::atomic_bool stop_ = true;
    std::thread watcher_;
};
