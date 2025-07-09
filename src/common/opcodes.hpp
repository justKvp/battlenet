#pragma once

enum class Opcode : uint16_t {
    PING = 1,
    PONG = 2,
    MESSAGE = 3
};