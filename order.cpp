//
// Created by Devang Jaiswal on 6/27/24.
//

#include "order.h"
#include <iomanip>
#include <sstream>

order::order(uint64_t id, float price, uint32_t size, bool side, uint64_t unix_time)
        : id_(id), size(size), price_(price), side_(side), unix_time_(unix_time), filled_(false), next_(nullptr), prev_(nullptr), parent_(nullptr) {
}
