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

    void write(const void* data, size_t size) {
        if (buffer_.size() + size > MAX_SIZE) {
            throw std::runtime_error("Buffer would exceed 1 MB limit");
        }
        const auto* bytes = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + size);
    }

    template<typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Type must be trivially copyable");
        write(&value, sizeof(T));
    }

    void read(void* dest, size_t size) const {
        if (read_pos_ + size > buffer_.size()) {
            throw std::out_of_range("ByteBuffer read out of range");
        }
        std::memcpy(dest, buffer_.data() + read_pos_, size);
        read_pos_ += size;
    }

    template<typename T>
    T read() const {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Type must be trivially copyable");
        T value;
        read(&value, sizeof(T));
        return value;
    }

    void write_string(const std::string& str) {
        if (str.size() > MAX_SIZE - sizeof(uint32_t)) {
            throw std::runtime_error("String size exceeds 1 MB limit");
        }
        uint32_t length = static_cast<uint32_t>(str.size());
        write(length);
        write(str.data(), length);
    }

    std::string read_string() const {
        uint32_t length = read<uint32_t>();
        if (length > MAX_SIZE - sizeof(uint32_t)) {
            throw std::runtime_error("String size exceeds 1 MB limit");
        }
        std::string result(length, '\0');
        read(&result[0], length);
        return result;
    }

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
};