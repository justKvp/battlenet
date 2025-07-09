#pragma once

#include <cstdint>

enum class Opcode : uint16_t {
    UNDEFINED = 0,

    CLIENT_AUTH_INIT_REQ = 1,
    SERVER_AUTH_INIT_RES = 2,

    CLIENT_HELLO_REQ     = 3,
    SERVER_HELLO_ANSWER  = 4,

    SERVER_INTERNAL_ERROR_RES = 5
};