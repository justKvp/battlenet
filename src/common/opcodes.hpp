#pragma once

enum class Opcode : uint16_t {
    MESSAGE = 1,

    CMSG_PING = 2,
    SMSG_PONG = 3,

    CMSG_HELLO = 4,
    SMSG_HELLO_RES = 5
};