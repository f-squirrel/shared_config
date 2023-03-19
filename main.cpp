#include "Config.hpp"

#include <iostream>

int main() {

    auto config_manager =
        ConfigManager{"path.txt", std::chrono::milliseconds{1000}};
    std::cout << config_manager.get().name() << std::endl;
    auto config = config_manager.get();
    std::ignore = config.name();
    return 0;
}
