#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

class ByteBuffer {
    std::vector<uint8_t> buffer_;
    mutable size_t read_pos_ = 0; // mutable для константных операций

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

    // Чтение данных (неконстантная версия)
    void read(void* dest, size_t size) {
        check_read_bound(size);
        memcpy(dest, buffer_.data() + read_pos_, size);
        read_pos_ += size;
    }

    template<typename T>
    T read() {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Type must be trivially copyable");
        T value;
        read(&value, sizeof(T));
        return value;
    }

    // Чтение данных (константная версия)
    void read(void* dest, size_t size) const {
        check_read_bound(size);
        memcpy(dest, buffer_.data() + read_pos_, size);
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

    // Специализированные методы для строк
    void write_string(const std::string& str) {
        write<uint32_t>(static_cast<uint32_t>(str.size()));
        write(str.data(), str.size());
    }

    std::string read_string() const {
        uint32_t size = read<uint32_t>();
        if (size > 1024 * 1024) { // Защита от слишком больших строк
            throw std::runtime_error("String size too large");
        }
        std::string result(size, '\0');
        read(&result[0], size);
        return result;
    }

    // Управление буфером
    void clear() {
        buffer_.clear();
        read_pos_ = 0;
    }

    // Доступ к данным
    const uint8_t* data() const { return buffer_.data(); }
    size_t size() const { return buffer_.size(); }
    size_t remaining() const { return buffer_.size() - read_pos_; }

private:
    void check_read_bound(size_t size) const {
        if (read_pos_ + size > buffer_.size()) {
            throw std::runtime_error("ByteBuffer underflow");
        }
    }
};