#pragma once

#include "ByteBuffer.hpp"
#include "opcodes.hpp"

struct Packet {
    uint8_t opcode;
    ByteBuffer buffer;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> out;

        uint32_t length = 1 + buffer.size(); // 1 byte opcode
        for (int i = 0; i < 4; ++i)
            out.push_back((length >> (i * 8)) & 0xFF);

        out.push_back(opcode);
        out.insert(out.end(), buffer.data().begin(), buffer.data().end());

        return out;
    }

    static Packet deserialize(const std::vector<uint8_t>& data) {
        if (data.size() < 1)
            throw std::runtime_error("No opcode in body");
        Packet p;
        p.opcode = data[0];
        p.buffer.append(data.data() + 1, data.size() - 1);
        return p;
    }
};
