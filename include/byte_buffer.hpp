#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <cstring>

class ByteBuffer {
    std::vector<uint8_t> buffer_;
    mutable size_t read_pos_ = 0;
    static constexpr size_t MAX_SIZE = 1024 * 1024; // 1 MB

public:
    ByteBuffer() = default;
    explicit ByteBuffer(size_t size) : buffer_(size) {
        if (size > MAX_SIZE) {
            throw std::runtime_error("Buffer size exceeds 1 MB limit");
        }
    }

    // Базовый метод записи
    void write(const void* data, size_t size) {
        if (buffer_.size() + size > MAX_SIZE) {
            throw std::runtime_error("Buffer would exceed 1 MB limit");
        }
        const auto* bytes = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + size);
    }

    // Базовый метод чтения
    void read(void* dest, size_t size) const {
        if (read_pos_ + size > buffer_.size()) {
            throw std::out_of_range("ByteBuffer read out of range");
        }
        std::memcpy(dest, buffer_.data() + read_pos_, size);
        read_pos_ += size;
    }

    //////////////////////////////////////////////////
    // Специализированные методы записи
    //////////////////////////////////////////////////
    void write_uint8(uint8_t value)  { write(&value, sizeof(value)); }
    void write_uint16(uint16_t value) { write(&value, sizeof(value)); }
    void write_uint32(uint32_t value) { write(&value, sizeof(value)); }
    void write_uint64(uint64_t value) { write(&value, sizeof(value)); }
    void write_int8(int8_t value)    { write(&value, sizeof(value)); }
    void write_int16(int16_t value)  { write(&value, sizeof(value)); }
    void write_int32(int32_t value)  { write(&value, sizeof(value)); }
    void write_int64(int64_t value)  { write(&value, sizeof(value)); }
    void write_float(float value)    { write(&value, sizeof(value)); }
    void write_double(double value)  { write(&value, sizeof(value)); }
    void write_bool(bool value)      { write_uint8(value ? 1 : 0); }

    //////////////////////////////////////////////////
    // Специализированные методы чтения
    //////////////////////////////////////////////////
    uint8_t read_uint8() const  { uint8_t v; read(&v, sizeof(v)); return v; }
    uint16_t read_uint16() const { uint16_t v; read(&v, sizeof(v)); return v; }
    uint32_t read_uint32() const { uint32_t v; read(&v, sizeof(v)); return v; }
    uint64_t read_uint64() const { uint64_t v; read(&v, sizeof(v)); return v; }
    int8_t read_int8() const    { int8_t v; read(&v, sizeof(v)); return v; }
    int16_t read_int16() const  { int16_t v; read(&v, sizeof(v)); return v; }
    int32_t read_int32() const  { int32_t v; read(&v, sizeof(v)); return v; }
    int64_t read_int64() const  { int64_t v; read(&v, sizeof(v)); return v; }
    float read_float() const    { float v; read(&v, sizeof(v)); return v; }
    double read_double() const  { double v; read(&v, sizeof(v)); return v; }
    bool read_bool() const      { return read_uint8() != 0; }

    //////////////////////////////////////////////////
    // Работа со строками
    //////////////////////////////////////////////////
    void write_string(const std::string& str) {
        if (str.size() > MAX_SIZE - sizeof(uint32_t)) {
            throw std::runtime_error("String size exceeds 1 MB limit");
        }
        write_uint32(static_cast<uint32_t>(str.size()));
        write(str.data(), str.size());
    }

    std::string read_string() const {
        uint32_t length = read_uint32();
        if (length > MAX_SIZE - sizeof(uint32_t)) {
            throw std::runtime_error("String size exceeds 1 MB limit");
        }
        std::string result(length, '\0');
        read(&result[0], length);
        return result;
    }

    //////////////////////////////////////////////////
    // Управление буфером
    //////////////////////////////////////////////////
    void append(const ByteBuffer& other) {
        if (buffer_.size() + other.buffer_.size() > MAX_SIZE) {
            throw std::runtime_error("Append would exceed 1 MB limit");
        }
        buffer_.insert(buffer_.end(), other.buffer_.begin(), other.buffer_.end());
    }

    void clear() {
        buffer_.clear();
        read_pos_ = 0;
    }

    const uint8_t* data() const { return buffer_.data(); }
    size_t size() const { return buffer_.size(); }
    size_t remaining() const { return buffer_.size() - read_pos_; }
    void rewind() const { read_pos_ = 0; }
    bool empty() const { return buffer_.empty(); }

    //////////////////////////////////////////////////
    // Универсальные шаблонные методы (альтернатива)
    //////////////////////////////////////////////////
    template<typename T>
    void write(T value) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                      "Only arithmetic and enum types are supported");
        write(&value, sizeof(T));
    }

    template<typename T>
    T read() const {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                      "Only arithmetic and enum types are supported");
        T value;
        read(&value, sizeof(T));
        return value;
    }
};