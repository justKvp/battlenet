#include "opcodes.hpp"
#include "ByteBuffer.hpp"

struct Packet {
    Opcode opcode;
    ByteBuffer buffer;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> result;
        ByteBuffer temp = buffer;
        std::vector<uint8_t> body = {static_cast<uint8_t>(opcode)};
        body.insert(body.end(), temp.data.begin(), temp.data.end());

        uint16_t len = body.size();
        result.push_back(len >> 8);
        result.push_back(len & 0xFF);
        result.insert(result.end(), body.begin(), body.end());
        return result;
    }

    static Packet deserialize(const std::vector<uint8_t> &data) {
        if (data.empty()) throw std::runtime_error("Empty packet");
        Packet p;
        p.opcode = static_cast<Opcode>(data[0]);
        p.buffer.data = std::vector<uint8_t>(data.begin() + 1, data.end());
        return p;
    }
};