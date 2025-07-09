#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

class ByteBuffer {
public:
    std::vector<uint8_t> data;
    size_t offset = 0;

    void write_uint8(uint8_t v) { data.push_back(v); }

    void write_string(const std::string &str) {
        write_uint16(str.size());
        data.insert(data.end(), str.begin(), str.end());
    }

    void write_uint16(uint16_t v) {
        data.push_back(v >> 8);
        data.push_back(v & 0xFF);
    }

    uint8_t read_uint8() {
        if (offset >= data.size()) throw std::runtime_error("Buffer underflow");
        return data[offset++];
    }

    uint16_t read_uint16() {
        if (offset + 1 >= data.size()) throw std::runtime_error("Buffer underflow");
        uint16_t v = (data[offset] << 8) | data[offset + 1];
        offset += 2;
        return v;
    }

    std::string read_string() {
        uint16_t len = read_uint16();
        if (offset + len > data.size()) throw std::runtime_error("Buffer underflow");
        std::string str(data.begin() + offset, data.begin() + offset + len);
        offset += len;
        return str;
    }
};