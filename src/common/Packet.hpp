#pragma once

#include "ByteBuffer.hpp"
#include "opcodes.hpp"
#include "Logger.hpp"

struct Packet {
    uint8_t opcode;
    ByteBuffer buffer;

    // === Serialize ===
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> out;
        uint32_t len = 1 + buffer.size();
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
        out.push_back(opcode);
        out.insert(out.end(), buffer.data().begin(), buffer.data().end());

        // Можно оставить лог, но аккуратно:
        // Logger::get()->debug("[Packet::serialize] LEN={} OPCODE={} => SIZE={}", len, opcode, out.size());
        return out;
    }

    static Packet deserialize(const std::vector<uint8_t>& raw) {
        if (raw.empty()) {
            throw std::runtime_error("Empty packet in deserialize()");
        }

        Packet pkt;

        pkt.opcode = raw[0];

        if (raw.size() > 1) {
            pkt.buffer.append(raw.data() + 1, raw.size() - 1);
        }

        // 🔍 HEX FULL
        std::ostringstream oss;
        for (size_t i = 0; i < raw.size(); ++i) {
            oss << fmt::format("{:02X} ", raw[i]);
        }

        Logger::get()->debug("[Packet::deserialize] RAW FULL: {}", oss.str());
        Logger::get()->debug("[Packet::deserialize] OPCODE: {}", static_cast<int>(pkt.opcode));
        Logger::get()->debug("[Packet::deserialize] PAYLOAD LEN: {}", raw.size() - 1);

        return pkt;
    }
};
