#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <stdexcept>

class ByteBuffer {
public:
    ByteBuffer() : read_pos_(0) {}

    explicit ByteBuffer(const std::vector<uint8_t>& data) : buffer_(data), read_pos_(0) {}

    // === WRITE ===
    void write_uint8(uint8_t v) {
        buffer_.push_back(v);
    }

    void write_uint16(uint16_t v) {
        buffer_.push_back(v & 0xFF);
        buffer_.push_back((v >> 8) & 0xFF);
    }

    void write_uint32(uint32_t v) {
        buffer_.push_back(v & 0xFF);
        buffer_.push_back((v >> 8) & 0xFF);
        buffer_.push_back((v >> 16) & 0xFF);
        buffer_.push_back((v >> 24) & 0xFF);
    }

    void write_uint64(uint64_t v) {
        for (int i = 0; i < 8; ++i)
            buffer_.push_back((v >> (i * 8)) & 0xFF);
    }

    void write_int8(int8_t v) { write_uint8(static_cast<uint8_t>(v)); }
    void write_int16(int16_t v) { write_uint16(static_cast<uint16_t>(v)); }
    void write_int32(int32_t v) { write_uint32(static_cast<uint32_t>(v)); }
    void write_int64(int64_t v) { write_uint64(static_cast<uint64_t>(v)); }

    void write_bool(bool v) { write_uint8(v ? 1 : 0); }

    void write_float(float f) {
        uint32_t tmp;
        std::memcpy(&tmp, &f, sizeof(float));
        write_uint32(tmp);
    }

    void write_string(const std::string& str) {
        buffer_.insert(buffer_.end(), str.begin(), str.end());
        buffer_.push_back('\0');
    }

    // === READ ===
    uint8_t read_uint8() {
        check(1);
        return buffer_[read_pos_++];
    }

    uint16_t read_uint16() {
        check(2);
        uint16_t v = buffer_[read_pos_] |
                     (buffer_[read_pos_ + 1] << 8);
        read_pos_ += 2;
        return v;
    }

    uint32_t read_uint32() {
        check(4);
        uint32_t v = buffer_[read_pos_] |
                     (buffer_[read_pos_ + 1] << 8) |
                     (buffer_[read_pos_ + 2] << 16) |
                     (buffer_[read_pos_ + 3] << 24);
        read_pos_ += 4;
        return v;
    }

    uint64_t read_uint64() {
        check(8);
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= (static_cast<uint64_t>(buffer_[read_pos_ + i]) << (i * 8));
        }
        read_pos_ += 8;
        return v;
    }

    int8_t read_int8() { return static_cast<int8_t>(read_uint8()); }
    int16_t read_int16() { return static_cast<int16_t>(read_uint16()); }
    int32_t read_int32() { return static_cast<int32_t>(read_uint32()); }
    int64_t read_int64() { return static_cast<int64_t>(read_uint64()); }

    bool read_bool() { return read_uint8() != 0; }

    float read_float() {
        uint32_t tmp = read_uint32();
        float f;
        std::memcpy(&f, &tmp, sizeof(float));
        return f;
    }

    std::string read_string() {
        size_t start = read_pos_;
        while (read_pos_ < buffer_.size() && buffer_[read_pos_] != '\0')
            ++read_pos_;
        if (read_pos_ >= buffer_.size()) throw std::runtime_error("String not null-terminated");
        std::string s(buffer_.begin() + start, buffer_.begin() + read_pos_);
        ++read_pos_; // Skip null
        return s;
    }

    // === APPEND ===
    void append(const uint8_t* data, size_t len) {
        buffer_.insert(buffer_.end(), data, data + len);
    }

    const std::vector<uint8_t>& data() const { return buffer_; }
    size_t size() const { return buffer_.size(); }
    void clear() { buffer_.clear(); read_pos_ = 0; }

private:
    void check(size_t n) {
        if (read_pos_ + n > buffer_.size()) {
            throw std::runtime_error("Read past end of buffer");
        }
    }

    std::vector<uint8_t> buffer_;
    size_t read_pos_;
};
