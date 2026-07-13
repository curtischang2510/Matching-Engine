//
// Created by Curtis chang on 12/7/26.
//

#ifndef MARKETMAKER_BYTEREADER_H
#define MARKETMAKER_BYTEREADER_H
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

class ByteReader
{
public:
    ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size)
    {}

    std::uint8_t u8()
    {
        need(1);
        return data_[pos_++];
    }

    std::uint16_t u16()
    {
        need(2);
        std::uint16_t v = (static_cast<std::uint16_t>(data_[pos_]) << 8) | data_[pos_ + 1];
        pos_ += 2;
        return v;
    }

    std::uint32_t u32() {
        need(4);
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v = (v << 8) | data_[pos_ + i];
        pos_ += 4; return v;
    }

    std::uint64_t u64() {
        need(8);
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | data_[pos_ + i];
        pos_ += 8; return v;
    }

    void bytes(char* out, std::size_t n)
    {
        need(n);
        std::memcpy(out, data_ + pos_, n);
        pos_ += n;
    }

    const std::uint8_t* take(std::size_t n)
    {
        need(n);
        const std::uint8_t* p = data_ + pos_;
        pos_ += n;
        return p;
    }

    [[nodiscard]] std::size_t remaining() const { return size_ - pos_; }

private:
    void need(std::size_t n) const
    {
        if (pos_ + n > size_)
        {
            throw std::out_of_range("ByteReader: buffer underrun");
        }
    }

    const std::uint8_t* data_; // pointer to a raw byte buffer
    std::size_t size_;
    std::size_t pos_ = 0;
};

#endif //MARKETMAKER_BYTEREADER_H
