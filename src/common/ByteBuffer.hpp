#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <cstdint>

class ByteBuffer {
public:
    void write_uint8(uint8_t value) { buffer.push_back(value); }
    void write_uint16(uint16_t value) { write_bytes(&value, sizeof(value)); }
    void write_uint32(uint32_t value) { write_bytes(&value, sizeof(value)); }
    void write_uint64(uint64_t value) { write_bytes(&value, sizeof(value)); }

    void write_int8(int8_t value) { buffer.push_back(static_cast<uint8_t>(value)); }
    void write_int16(int16_t value) { write_bytes(&value, sizeof(value)); }
    void write_int32(int32_t value) { write_bytes(&value, sizeof(value)); }
    void write_int64(int64_t value) { write_bytes(&value, sizeof(value)); }

    void write_float(float value) { write_bytes(&value, sizeof(value)); }
    void write_double(double value) { write_bytes(&value, sizeof(value)); }

    void write_bool(bool value) { buffer.push_back(value ? 1 : 0); }

    void write_string(const std::string& str) {
        write_uint16(static_cast<uint16_t>(str.size()));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    uint8_t read_uint8() { return read_primitive<uint8_t>(); }
    uint16_t read_uint16() { return read_primitive<uint16_t>(); }
    uint32_t read_uint32() { return read_primitive<uint32_t>(); }
    uint64_t read_uint64() { return read_primitive<uint64_t>(); }

    int8_t read_int8() { return static_cast<int8_t>(read_primitive<uint8_t>()); }
    int16_t read_int16() { return read_primitive<int16_t>(); }
    int32_t read_int32() { return read_primitive<int32_t>(); }
    int64_t read_int64() { return read_primitive<int64_t>(); }

    float read_float() { return read_primitive<float>(); }
    double read_double() { return read_primitive<double>(); }

    bool read_bool() { return read_primitive<uint8_t>() != 0; }

    std::string read_string() {
        uint16_t len = read_uint16();
        if (read_pos + len > buffer.size()) throw std::runtime_error("String read out of bounds");
        std::string str(buffer.begin() + read_pos, buffer.begin() + read_pos + len);
        read_pos += len;
        return str;
    }

    const std::vector<uint8_t>& data() const { return buffer; }
    void load(const std::vector<uint8_t>& input) { buffer = input; read_pos = 0; }

private:
    std::vector<uint8_t> buffer;
    size_t read_pos = 0;

    template<typename T>
    void write_bytes(const T* val, size_t size) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(val);
        buffer.insert(buffer.end(), ptr, ptr + size);
    }

    template<typename T>
    T read_primitive() {
        if (read_pos + sizeof(T) > buffer.size()) throw std::runtime_error("Read out of bounds");
        T val;
        std::memcpy(&val, &buffer[read_pos], sizeof(T));
        read_pos += sizeof(T);
        return val;
    }
};