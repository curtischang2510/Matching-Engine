//
// Created by Curtis chang on 4/7/26.
//
#ifndef ORDERBOOK_SPSCBUFFER_H
#define ORDERBOOK_SPSCBUFFER_H

#include "LogEvent.h"

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <vector>


class SPSCBuffer
{
public:
   explicit SPSCBuffer(std::size_t capacity): capacity_(capacity)
   {
      if (capacity < 2)
      {
         throw std::invalid_argument("capacity must be at least 2");
      }

      buffer.resize(capacity);
      head_ = 0; // outgoing
      tail_ = 0; // incoming
   }

   bool try_push(const LogEvent& log) // push to tail
   {
      const std::size_t t = tail_.load(std::memory_order::relaxed);
      const std::size_t nextTail = nextIndex(t);
      const std::size_t h = head_.load(std::memory_order::acquire);
      if (nextTail == h)
      {
         return false;
      }
      buffer[t] = log; // copy

      // must be done at the end
      tail_.store(nextTail, std::memory_order_release);
      return true;
   }

   bool try_pop(LogEvent& out) // pop from head
   {
      const std::size_t t = tail_.load(std::memory_order::acquire);
      const std::size_t h = head_.load(std::memory_order::relaxed);
      if (t == h)
      {
         return false;
      }
      out = buffer[h]; // copy (if not copy this can result in race condition)

      // must be done at the very end if not there is a race condition with try_push
      head_.store(nextIndex(h), std::memory_order_release);
      return true;
   }

private:
   std::vector<LogEvent>  buffer;
   std::size_t            capacity_;
   alignas(64) std::atomic<std::size_t> head_{0}; // padding added to prevent false sharing (potentially wasteful)
   alignas(64) std::atomic<std::size_t> tail_{0};

   [[nodiscard]] std::size_t nextIndex(std::size_t index) const
   {
      ++index;
      return index == capacity_ ? 0 : index;
   }
};

#endif //ORDERBOOK_SPSCBUFFER_H
