#include "Config.hpp"

#include <iostream>

int main() {

    auto config_manager =
        ConfigManager{"path.txt", std::chrono::milliseconds{1000}};
    std::cout << config_manager.get().name() << std::endl;
    return 0;
}
