#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H

#include <vector>
#include <cstdint>
#include <cstring>

class ByteBuffer {
public:
    // Конструкторы
    explicit ByteBuffer(size_t size = 1024) : buffer_(size), position_(0) {}

    ByteBuffer(const ByteBuffer&) = default;
    ByteBuffer& operator=(const ByteBuffer&) = default;

    // Основные методы
    void WriteInt32(int32_t value) {
        EnsureSpace(sizeof(value));
        std::memcpy(buffer_.data() + position_, &value, sizeof(value));
        position_ += sizeof(value);
    }

    void WriteString(const std::string& str) {
        WriteInt32(static_cast<int32_t>(str.size()));
        EnsureSpace(str.size());
        std::memcpy(buffer_.data() + position_, str.data(), str.size());
        position_ += str.size();
    }

    int32_t ReadInt32() {
        int32_t value;
        std::memcpy(&value, buffer_.data() + position_, sizeof(value));
        position_ += sizeof(value);
        return value; // Должен возвращать то же значение, что было записано
    }

    std::string ReadString() {
        int32_t length = ReadInt32();
        std::string str(buffer_.data() + position_, length);
        position_ += length;
        return str;
    }

    // Доступ к данным
    const char* Data() const { return buffer_.data(); }
    char* Data() { return buffer_.data(); }
    size_t Size() const { return position_; }
    size_t Capacity() const { return buffer_.size(); }
    void SetSize(size_t size) { position_ = size; }
    void Clear() { position_ = 0; }

private:
    void EnsureSpace(size_t needed) {
        if (position_ + needed > buffer_.size()) {
            buffer_.resize(position_ + needed);
        }
    }

    std::vector<char> buffer_;
    size_t position_;
};

#endif // BYTE_BUFFER_H