#ifndef MARKETMAKER_LOCALORDERBOOK_H
#define MARKETMAKER_LOCALORDERBOOK_H

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>

#include "ITCH/itch.h"

class LocalOrderBook
{
public:
    void apply(const AddOrder& m);
    void apply(const OrderExecuted& m);
    void apply(const OrderCancel& m);
    void apply(const OrderDelete& m);

    struct L1
    {
        std::optional<std::uint32_t> bidPrice;
        std::uint64_t bidQty = 0;
        std::optional<std::uint32_t> askPrice;
        std::uint64_t askQty = 0;
    };

    L1 top() const;
    std::size_t orderCount() const { return index_.size(); }
    void printL2(std::size_t depth = 5) const;

private:
    struct Order
    {
        std::uint64_t id;
        std::uint32_t shares;
    };

    struct Level
    {
        std::uint32_t price = 0;
        std::uint64_t totalQty = 0;
        std::list<Order> orders;
    };

    struct Locator
    {
        Side side;
        std::uint32_t price;
        std::list<Order>::iterator it;
    };

    std::map<std::uint32_t, Level> bids_;
    std::map<std::uint32_t, Level> asks_;
    std::unordered_map<std::uint64_t, Locator> index_;

    std::map<std::uint32_t, Level>& sideMap(Side s) { return s == Side::BUY ? bids_ : asks_; }
    const std::map<std::uint32_t, Level>& sideMap(Side s) const { return s == Side::BUY ? bids_ : asks_; }

    void reduce(std::uint64_t id, std::uint32_t by);
};

#endif //MARKETMAKER_LOCALORDERBOOK_H
