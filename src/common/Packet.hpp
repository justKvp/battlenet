#pragma once

#include <iostream>
#include <vector>
#include "opcodes.hpp"
#include "ByteBuffer.hpp"

struct Packet {
    Opcode opcode;
    ByteBuffer buffer;

    std::vector<uint8_t> serialize() const {
        ByteBuffer buffer_out;
        buffer_out.write_uint16(static_cast<uint16_t>(opcode));
        const auto& payload = buffer.data();
        buffer_out.data().insert(buffer_out.data().end(), payload.begin(), payload.end());
        return buffer_out.data();
    }

    static Packet deserialize(const std::vector<uint8_t>& data) {
        if (data.size() < 2) {
            throw std::runtime_error("Packet too short to contain opcode");
        }

        ByteBuffer buffer(data);
        Packet pkt;
        pkt.opcode = static_cast<Opcode>(buffer.read_uint16());

        if (data.size() > 2) {
            pkt.buffer = ByteBuffer(std::vector<uint8_t>(data.begin() + 2, data.end()));
        }

        return pkt;
    }
};
