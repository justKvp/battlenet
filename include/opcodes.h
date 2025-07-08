#pragma once

#include <cstdint>

enum class Opcodes : uint16_t {
    CMSG_INVENTORY_MOVE = 0x1001,
    SMSG_INVENTORY_MOVE_RESULT = 0x2001
};