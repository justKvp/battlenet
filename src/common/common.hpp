#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <stdexcept>
#include <cstring>

namespace opcodes {
    // Базовые OP-коды
    constexpr uint32_t HELLO = 0x48454C4C;     // 'HELL'
    constexpr uint32_t HOW_ARE_YOU = 0x484F573F; // 'HOW?'
    constexpr uint32_t RESPONSE = 0x52455350;   // 'RESP'
    constexpr uint32_t DATA_PAIR = 0x44415441;  // 'DATA'

    // Дополнительные типы сообщений
    constexpr uint32_t BINARY_DATA = 0x42494E44; // 'BIND'
}

// Поддерживаемые типы полей
using FieldValue = std::variant<
        uint8_t, uint16_t, uint32_t, uint64_t,
        int8_t, int16_t, int32_t, int64_t,
        float, double,
        std::string,
        std::vector<uint8_t>
>;

struct Message {
    uint32_t opcode;
    std::map<std::string, FieldValue> fields;
};

class MessageBuilder {
public:
    enum FieldType : uint8_t {
        UINT8 = 0,
        UINT16 = 1,
        UINT32 = 2,
        UINT64 = 3,
        INT8 = 4,
        INT16 = 5,
        INT32 = 6,
        INT64 = 7,
        FLOAT = 8,
        DOUBLE = 9,
        STRING = 10,
        BINARY = 11
    };

    static std::vector<uint8_t> serialize(const Message& msg) {
        std::vector<uint8_t> buffer;

        append_to_buffer(buffer, msg.opcode);
        append_to_buffer(buffer, static_cast<uint32_t>(msg.fields.size()));

        for (const auto& [name, value] : msg.fields) {
            append_to_buffer(buffer, static_cast<uint32_t>(name.size()));
            buffer.insert(buffer.end(), name.begin(), name.end());

            std::visit([&buffer](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, uint8_t>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(UINT8));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, uint16_t>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(UINT16));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, uint32_t>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(UINT32));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, uint64_t>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(UINT64));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, int8_t>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(INT8));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, int16_t>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(INT16));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, int32_t>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(INT32));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, int64_t>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(INT64));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, float>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(FLOAT));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, double>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(DOUBLE));
                    append_to_buffer(buffer, arg);
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(STRING));
                    append_to_buffer(buffer, static_cast<uint32_t>(arg.size()));
                    buffer.insert(buffer.end(), arg.begin(), arg.end());
                }
                else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    append_to_buffer(buffer, static_cast<uint8_t>(BINARY));
                    append_to_buffer(buffer, static_cast<uint32_t>(arg.size()));
                    buffer.insert(buffer.end(), arg.begin(), arg.end());
                }
            }, value);
        }
        return buffer;
    }

    static Message deserialize(const std::vector<uint8_t>& buffer) {
        Message msg;
        size_t offset = 0;

        msg.opcode = read_from_buffer<uint32_t>(buffer, offset);
        uint32_t field_count = read_from_buffer<uint32_t>(buffer, offset);

        for (uint32_t i = 0; i < field_count; ++i) {
            uint32_t name_length = read_from_buffer<uint32_t>(buffer, offset);
            std::string name(
                    reinterpret_cast<const char*>(buffer.data() + offset),
                    name_length
            );
            offset += name_length;

            FieldType type = static_cast<FieldType>(
                    read_from_buffer<uint8_t>(buffer, offset)
            );

            FieldValue value;
            switch (type) {
                case UINT8:  value = read_from_buffer<uint8_t>(buffer, offset); break;
                case UINT16: value = read_from_buffer<uint16_t>(buffer, offset); break;
                case UINT32: value = read_from_buffer<uint32_t>(buffer, offset); break;
                case UINT64: value = read_from_buffer<uint64_t>(buffer, offset); break;
                case INT8:   value = read_from_buffer<int8_t>(buffer, offset); break;
                case INT16:  value = read_from_buffer<int16_t>(buffer, offset); break;
                case INT32:  value = read_from_buffer<int32_t>(buffer, offset); break;
                case INT64:  value = read_from_buffer<int64_t>(buffer, offset); break;
                case FLOAT:  value = read_from_buffer<float>(buffer, offset); break;
                case DOUBLE: value = read_from_buffer<double>(buffer, offset); break;
                case STRING: {
                    uint32_t str_length = read_from_buffer<uint32_t>(buffer, offset);
                    value = std::string(
                            reinterpret_cast<const char*>(buffer.data() + offset),
                            str_length
                    );
                    offset += str_length;
                    break;
                }
                case BINARY: {
                    uint32_t bin_length = read_from_buffer<uint32_t>(buffer, offset);
                    std::vector<uint8_t> bin_data(
                            buffer.begin() + offset,
                            buffer.begin() + offset + bin_length
                    );
                    value = bin_data;
                    offset += bin_length;
                    break;
                }
                default: throw std::runtime_error("Unknown field type");
            }
            msg.fields.emplace(std::move(name), std::move(value));
        }
        return msg;
    }

private:
    template<typename T>
    static void append_to_buffer(std::vector<uint8_t>& buffer, const T& value) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    }

    template<typename T>
    static T read_from_buffer(const std::vector<uint8_t>& buffer, size_t& offset) {
        if (offset + sizeof(T) > buffer.size()) {
            throw std::runtime_error("Buffer underflow");
        }
        T value;
        std::memcpy(&value, buffer.data() + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }
};