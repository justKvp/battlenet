#pragma once
#include "ByteBuffer.hpp"
#include "opcodes.hpp"

struct Packet {
    Opcode opcode;
    ByteBuffer buffer;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(opcode));
        auto payload = buffer.data();
        data.insert(data.end(), payload.begin(), payload.end());
        return data;
    }

    static Packet deserialize(const std::vector<uint8_t>& data) {
        if (data.empty()) throw std::runtime_error("Empty packet");
        Packet p;
        p.opcode = static_cast<Opcode>(data[0]);
        p.buffer = ByteBuffer(std::vector<uint8_t>(data.begin() + 1, data.end()));
        return p;
    }
};
