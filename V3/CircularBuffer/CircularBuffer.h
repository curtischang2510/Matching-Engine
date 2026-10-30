//
// Created by Curtis chang on 28/6/26.
//
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#ifndef ORDERBOOK_CIRCULARBUFFER_H
#define ORDERBOOK_CIRCULARBUFFER_H



template <typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(size_t capacity): capacity(capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("capacity must be greater than 0");
        }

        buffer.resize(capacity);
        numElements = 0;
        frontPointer = 0;
        rearPointer = 0;
    };

    [[nodiscard]] size_t size() const
    {
        return numElements;
    };

    [[nodiscard]] bool empty() const
    {
        return numElements == 0;
    };

    // remove front (make this similar to other C++ DS where pop_front() returns void and we need to call front
    // add rear
    T& front()
    {
        if (numElements == 0)
        {
            throw std::out_of_range("buffer is empty");
        }

        return buffer[frontPointer];
    };

    void pop_front()
    {
        if (numElements == 0)
        {
            return;
        }
        frontPointer = addIdx(frontPointer, 1);
        --numElements;
    };

    void push_back(const T& val)
    {
        // if numElements == capacity, need to double in size
        if (numElements == capacity)
        {
            allocate();
        }

        buffer[rearPointer] = val;
        rearPointer = addIdx(rearPointer, 1);
        ++numElements;
    };

    void push_back(T&& val)
    {
        if (numElements == capacity)
        {
            allocate();
        }

        buffer[rearPointer] = std::move(val);
        rearPointer = addIdx(rearPointer, 1);
        ++numElements;
    }

private:
    size_t capacity;
    size_t numElements;
    size_t frontPointer;
    size_t rearPointer;
    std::vector<T> buffer;

    [[nodiscard]] size_t addIdx(const size_t currIdx, const size_t numIncrements) const
    {
        return (currIdx + numIncrements) % capacity;
    }

    void allocate()
    {
        // double buffer size
        size_t newCapacity = capacity * 2;
        buffer.resize(newCapacity);
        for (size_t i = 0; i < frontPointer; ++i)
        {
            buffer[capacity + i] = std::move(buffer[i]);
        }
        rearPointer = capacity + frontPointer;
        capacity = newCapacity;
    }
};



#endif //ORDERBOOK_CIRCULARBUFFER_H
