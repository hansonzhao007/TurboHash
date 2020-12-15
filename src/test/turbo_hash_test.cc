#include "turbo/turbo_hash.h"

int main() {

    turbo::unordered_map map;

    for (int i = 0; i < 128; i++) {
        std::string key = "key" + std::to_string(i);
        map.Put(key, "value");
    }

    for (int i = 0; i < 128; i++) {
        std::string value;
        std::string key = "key" + std::to_string(i);
        map.Get(key, &value);

        printf("Get key: %s, value: %s\n", key.c_str(), value.c_str());
    }

    return 0;
}