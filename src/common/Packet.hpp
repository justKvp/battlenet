#include "opcodes.hpp"
#include "ByteBuffer.hpp"

struct Packet {
    Opcode opcode;
    ByteBuffer buffer;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(opcode));
        const auto& buf = buffer.data();
        uint16_t size = static_cast<uint16_t>(1 + buf.size());
        data.insert(data.begin(), (size >> 8) & 0xFF);
        data.insert(data.begin() + 1, size & 0xFF);
        data.insert(data.begin() + 2, buf.begin(), buf.end());
        return data;
    }

    static Packet deserialize(const std::vector<uint8_t>& raw) {
        if (raw.size() < 1) throw std::runtime_error("Packet too short");
        Packet p;
        p.opcode = static_cast<Opcode>(raw[0]);
        std::vector<uint8_t> buf(raw.begin() + 1, raw.end());
        p.buffer.load(buf);
        return p;
    }
};
