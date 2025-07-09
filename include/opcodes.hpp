#pragma once

#include <cstdint>

enum class Opcode : uint16_t {
    UNDEFINED = 0,
    AUTH_REQUEST = 1,
    AUTH_RESPONSE = 2,
    DATA_REQUEST = 3,
    DATA_RESPONSE = 4
};