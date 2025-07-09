#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

class ByteBuffer {
public:
    ByteBuffer() : position_(0) {}

    explicit ByteBuffer(const std::vector<uint8_t>& data)
            : buffer_(data), position_(0) {}

    const std::vector<uint8_t>& data() const { return buffer_; }
    std::vector<uint8_t>& data() { return buffer_; }

    void write_uint8(uint8_t value) { buffer_.push_back(value); }
    void write_uint16(uint16_t value) {
        buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
    }
    void write_uint32(uint32_t value) {
        for (int i = 3; i >= 0; --i) {
            buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
    void write_uint64(uint64_t value) {
        for (int i = 7; i >= 0; --i) {
            buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    void write_int8(int8_t value) { write_uint8(static_cast<uint8_t>(value)); }
    void write_int16(int16_t value) { write_uint16(static_cast<uint16_t>(value)); }
    void write_int32(int32_t value) { write_uint32(static_cast<uint32_t>(value)); }
    void write_int64(int64_t value) { write_uint64(static_cast<uint64_t>(value)); }

    void write_float(float value) {
        uint32_t temp;
        std::memcpy(&temp, &value, sizeof(float));
        write_uint32(temp);
    }

    void write_double(double value) {
        uint64_t temp;
        std::memcpy(&temp, &value, sizeof(double));
        write_uint64(temp);
    }

    void write_bool(bool value) { write_uint8(value ? 1 : 0); }

    void write_string(const std::string& str) {
        write_uint16(static_cast<uint16_t>(str.size()));
        buffer_.insert(buffer_.end(), str.begin(), str.end());
    }

    uint8_t read_uint8() {
        check_size(1);
        return buffer_[position_++];
    }

    uint16_t read_uint16() {
        check_size(2);
        uint16_t value = (buffer_[position_] << 8) | buffer_[position_ + 1];
        position_ += 2;
        return value;
    }

    uint32_t read_uint32() {
        check_size(4);
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value = (value << 8) | buffer_[position_++];
        }
        return value;
    }

    uint64_t read_uint64() {
        check_size(8);
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | buffer_[position_++];
        }
        return value;
    }

    int8_t read_int8() { return static_cast<int8_t>(read_uint8()); }
    int16_t read_int16() { return static_cast<int16_t>(read_uint16()); }
    int32_t read_int32() { return static_cast<int32_t>(read_uint32()); }
    int64_t read_int64() { return static_cast<int64_t>(read_uint64()); }

    float read_float() {
        uint32_t temp = read_uint32();
        float value;
        std::memcpy(&value, &temp, sizeof(float));
        return value;
    }

    double read_double() {
        uint64_t temp = read_uint64();
        double value;
        std::memcpy(&value, &temp, sizeof(double));
        return value;
    }

    bool read_bool() {
        return read_uint8() != 0;
    }

    std::string read_string() {
        uint16_t length = read_uint16();
        check_size(length);
        std::string str(buffer_.begin() + position_, buffer_.begin() + position_ + length);
        position_ += length;
        return str;
    }

private:
    std::vector<uint8_t> buffer_;
    std::size_t position_;

    void check_size(std::size_t size) {
        if (position_ + size > buffer_.size()) {
            throw std::runtime_error("ByteBuffer: Not enough data to read");
        }
    }
};
