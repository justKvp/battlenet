// byte_buffer.h
#pragma once

#include <vector>
#include <cstring>    // Для memcpy
#include <stdexcept>  // Для исключений
#include <type_traits>// Для static_assert
#include <string>
#include <cstdint>    // Для uint8_t, uint16_t и т.д.

class ByteBuffer {
public:
    // Конструкторы
    ByteBuffer() : _rpos(0), _wpos(0) {}
    ByteBuffer(const uint8_t* data, size_t size) : _rpos(0), _wpos(0) {
        Append(data, size);
    }

    // Методы записи
    void Append(const uint8_t* data, size_t size) {
        if (size) {
            _storage.insert(_storage.end(), data, data + size);
            _wpos += size;
        }
    }

    template<typename T>
    ByteBuffer& operator<<(T value) {
        static_assert(std::is_fundamental<T>::value || std::is_enum<T>::value,
                      "ByteBuffer::operator<< can only write fundamental or enum types");

        size_t pos = _storage.size();
        _storage.resize(pos + sizeof(T));
        std::memcpy(&_storage[pos], &value, sizeof(T));
        _wpos += sizeof(T);
        return *this;
    }

    ByteBuffer& operator<<(const std::string& value) {
        uint16_t length = static_cast<uint16_t>(value.size());
        *this << length;

        size_t pos = _storage.size();
        _storage.resize(pos + length);
        std::memcpy(&_storage[pos], value.data(), length);
        _wpos += length;
        return *this;
    }

    // Методы чтения
    template<typename T>
    ByteBuffer& operator>>(T& value) {
        static_assert(std::is_fundamental<T>::value || std::is_enum<T>::value,
                      "ByteBuffer::operator>> can only read fundamental or enum types");

        if (!CanRead<T>())
            throw std::out_of_range("ByteBuffer: read out of range");

        value = *reinterpret_cast<T*>(&_storage[_rpos]);
        _rpos += sizeof(T);
        return *this;
    }

    ByteBuffer& operator>>(std::string& value) {
        uint16_t length = 0;
        *this >> length;

        if (!CanRead(length))
            throw std::out_of_range("ByteBuffer: string read out of range");

        value.assign(reinterpret_cast<const char*>(&_storage[_rpos]), length);
        _rpos += length;
        return *this;
    }

    // Утилиты
    const uint8_t* GetData() const { return _storage.data(); }
    size_t GetSize() const { return _storage.size(); }
    const uint8_t* GetReadPointer() const { return &_storage[_rpos]; }
    size_t GetActiveSize() const { return _storage.size() - _rpos; }
    void Clear() { _storage.clear(); _rpos = 0; _wpos = 0; }

    void SkipRead(size_t bytes) {
        if (_rpos + bytes > _storage.size())
            throw std::out_of_range("ByteBuffer: skip out of range");
        _rpos += bytes;
    }

    // Состояние
    bool Empty() const { return _storage.empty(); }
    size_t GetReadPos() const { return _rpos; }
    size_t GetWritePos() const { return _wpos; }

private:
    // Внутренние проверки
    template<typename T>
    bool CanRead() const {
        return sizeof(T) <= (_storage.size() - _rpos);
    }

    bool CanRead(size_t bytes) const {
        return bytes <= (_storage.size() - _rpos);
    }

    // Хранилище данных
    std::vector<uint8_t> _storage;
    size_t _rpos = 0; // Позиция чтения
    size_t _wpos = 0; // Позиция записи
};