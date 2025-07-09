#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <cstring>

class ByteBuffer {
public:
    ByteBuffer() : read_pos_(0) {}

    ByteBuffer(const std::vector<uint8_t>& data)
            : buffer_(data), read_pos_(0) {}

    const std::vector<uint8_t>& data() const { return buffer_; }

    void write_uint8(uint8_t value) { buffer_.push_back(value); }

    void write_uint16(uint16_t value) {
        buffer_.push_back((value >> 8) & 0xFF);
        buffer_.push_back(value & 0xFF);
    }

    void write_uint32(uint32_t value) {
        for (int i = 3; i >= 0; --i) {
            buffer_.push_back((value >> (8 * i)) & 0xFF);
        }
    }

    void write_uint64(uint64_t value) {
        for (int i = 7; i >= 0; --i) {
            buffer_.push_back((value >> (8 * i)) & 0xFF);
        }
    }

    void write_int8(int8_t value) { write_uint8(static_cast<uint8_t>(value)); }

    void write_int16(int16_t value) { write_uint16(static_cast<uint16_t>(value)); }

    void write_int32(int32_t value) { write_uint32(static_cast<uint32_t>(value)); }

    void write_int64(int64_t value) { write_uint64(static_cast<uint64_t>(value)); }

    void write_float(float value) {
        uint32_t tmp;
        std::memcpy(&tmp, &value, sizeof(tmp));
        write_uint32(tmp);
    }

    void write_double(double value) {
        uint64_t tmp;
        std::memcpy(&tmp, &value, sizeof(tmp));
        write_uint64(tmp);
    }

    void write_bool(bool value) { write_uint8(value ? 1 : 0); }

    void write_string(const std::string& str) {
        write_uint16(static_cast<uint16_t>(str.size()));
        buffer_.insert(buffer_.end(), str.begin(), str.end());
    }

    // -------------------------------
    uint8_t read_uint8() {
        ensure_available(1);
        return buffer_[read_pos_++];
    }

    uint16_t read_uint16() {
        ensure_available(2);
        uint16_t value = (buffer_[read_pos_] << 8) | buffer_[read_pos_ + 1];
        read_pos_ += 2;
        return value;
    }

    uint32_t read_uint32() {
        ensure_available(4);
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value = (value << 8) | buffer_[read_pos_ + i];
        }
        read_pos_ += 4;
        return value;
    }

    uint64_t read_uint64() {
        ensure_available(8);
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | buffer_[read_pos_ + i];
        }
        read_pos_ += 8;
        return value;
    }

    int8_t read_int8() { return static_cast<int8_t>(read_uint8()); }

    int16_t read_int16() { return static_cast<int16_t>(read_uint16()); }

    int32_t read_int32() { return static_cast<int32_t>(read_uint32()); }

    int64_t read_int64() { return static_cast<int64_t>(read_uint64()); }

    float read_float() {
        uint32_t tmp = read_uint32();
        float value;
        std::memcpy(&value, &tmp, sizeof(value));
        return value;
    }

    double read_double() {
        uint64_t tmp = read_uint64();
        double value;
        std::memcpy(&value, &tmp, sizeof(value));
        return value;
    }

    bool read_bool() {
        return read_uint8() != 0;
    }

    std::string read_string() {
        uint16_t len = read_uint16();
        ensure_available(len);
        std::string str(buffer_.begin() + read_pos_, buffer_.begin() + read_pos_ + len);
        read_pos_ += len;
        return str;
    }

private:
    std::vector<uint8_t> buffer_;
    size_t read_pos_;

    void ensure_available(size_t size) const {
        if (read_pos_ + size > buffer_.size()) {
            throw std::out_of_range("ByteBuffer: Not enough data");
        }
    }
};
