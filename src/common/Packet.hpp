#include <iostream>
#include "opcodes.hpp"
#include "ByteBuffer.hpp"

struct Packet {
    Opcode opcode;
    ByteBuffer buffer;

    // Сериализация в байты: [size_hi][size_lo][opcode][payload]
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> result;

        // 1) Сначала пишем opcode (1 байт)
        result.push_back(static_cast<uint8_t>(opcode));

        // 2) Потом все данные ByteBuffer
        const auto &payload = buffer.data();
        result.insert(result.end(), payload.begin(), payload.end());

        // 3) Длина пакета = opcode + payload
        uint16_t size = result.size();

        // 4) Полный пакет = [size_hi][size_lo][opcode][payload]
        std::vector<uint8_t> full_packet;
        full_packet.push_back((size >> 8) & 0xFF);
        full_packet.push_back(size & 0xFF);
        full_packet.insert(full_packet.end(), result.begin(), result.end());

        return full_packet;
    }

    // Десериализация из байтов: data без size (только [opcode][payload])
    static Packet deserialize(const std::vector<uint8_t> &data) {
        if (data.empty()) {
            throw std::runtime_error("Empty packet");
        }

        Packet packet;

        // Первый байт — это opcode
        packet.opcode = static_cast<Opcode>(data[0]);

        // Остальное — payload
        std::vector<uint8_t> payload(data.begin() + 1, data.end());
        packet.buffer = ByteBuffer(payload);

        // DEBUG:
        //std::cout << "[Packet] Deserialized opcode: " << static_cast<int>(packet.opcode)
        //          << ", payload size: " << payload.size() << "\n";

        return packet;
    }
};