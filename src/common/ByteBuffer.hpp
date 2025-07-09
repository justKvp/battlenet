#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>

class ByteBuffer {
public:
    ByteBuffer() = default;
    ByteBuffer(const std::vector<uint8_t>& data) : buffer(data), read_pos(0) {}

    const std::vector<uint8_t>& data() const { return buffer; }

    void write_uint8(uint8_t v)    { buffer.push_back(v); }
    void write_uint16(uint16_t v)  { write_integral(v); }
    void write_uint32(uint32_t v)  { write_integral(v); }
    void write_uint64(uint64_t v)  { write_integral(v); }
    void write_int8(int8_t v)      { buffer.push_back(static_cast<uint8_t>(v)); }
    void write_int16(int16_t v)    { write_integral(v); }
    void write_int32(int32_t v)    { write_integral(v); }
    void write_int64(int64_t v)    { write_integral(v); }
    void write_float(float v)      { write_integral(v); }
    void write_double(double v)    { write_integral(v); }
    void write_bool(bool v)        { write_uint8(v ? 1 : 0); }

    void write_string(const std::string& str) {
        write_uint32(static_cast<uint32_t>(str.size()));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    uint8_t  read_uint8()  { return read_integral<uint8_t>(); }
    uint16_t read_uint16() { return read_integral<uint16_t>(); }
    uint32_t read_uint32() { return read_integral<uint32_t>(); }
    uint64_t read_uint64() { return read_integral<uint64_t>(); }
    int8_t   read_int8()   { return static_cast<int8_t>(read_uint8()); }
    int16_t  read_int16()  { return read_integral<int16_t>(); }
    int32_t  read_int32()  { return read_integral<int32_t>(); }
    int64_t  read_int64()  { return read_integral<int64_t>(); }
    float    read_float()  { return read_integral<float>(); }
    double   read_double() { return read_integral<double>(); }
    bool     read_bool()   { return read_uint8() != 0; }

    std::string read_string() {
        uint32_t size = read_uint32();
        if (read_pos + size > buffer.size()) throw std::runtime_error("Buffer overflow");
        std::string str(buffer.begin() + read_pos, buffer.begin() + read_pos + size);
        read_pos += size;
        return str;
    }

private:
    std::vector<uint8_t> buffer;
    size_t read_pos = 0;

    template<typename T>
    void write_integral(T val) {
        uint8_t* p = reinterpret_cast<uint8_t*>(&val);
        buffer.insert(buffer.end(), p, p + sizeof(T));
    }

    template<typename T>
    T read_integral() {
        if (read_pos + sizeof(T) > buffer.size()) throw std::runtime_error("Buffer underflow");
        T val;
        std::memcpy(&val, buffer.data() + read_pos, sizeof(T));
        read_pos += sizeof(T);
        return val;
    }
};
