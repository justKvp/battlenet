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

public:
    ByteBuffer() = default;
    explicit ByteBuffer(size_t size) : buffer_(size) {}

    // Запись данных
    void write(const void* data, size_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + size);
    }

    template<typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Type must be trivially copyable");
        write(&value, sizeof(T));
    }

    // Чтение данных
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

    // Строковые операции
    void write_string(const std::string& str) {
        uint32_t length = static_cast<uint32_t>(str.size());
        write(length);
        write(str.data(), length);
    }

    std::string read_string() const {
        uint32_t length = read<uint32_t>();
        if (length > 1024*1024) {
            throw std::runtime_error("String size too large");
        }
        std::string result(length, '\0');
        read(&result[0], length);
        return result;
    }

    // Управление буфером
    void clear() {
        buffer_.clear();
        read_pos_ = 0;
    }

    const uint8_t* data() const { return buffer_.data(); }
    size_t size() const { return buffer_.size(); }
    size_t remaining() const { return buffer_.size() - read_pos_; }
    void rewind() const { read_pos_ = 0; }

    // Добавленные предложения:
    void append(const ByteBuffer& other) {
        buffer_.insert(buffer_.end(), other.buffer_.begin(), other.buffer_.end());
    }

    bool empty() const { return buffer_.empty(); }
};