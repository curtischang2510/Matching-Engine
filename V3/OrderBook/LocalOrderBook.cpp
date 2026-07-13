#include "OrderBook/LocalOrderBook.h"

#include <algorithm>
#include <iostream>

void LocalOrderBook::apply(const AddOrder& m)
{
    Level& lvl = sideMap(m.side)[m.price];
    lvl.price = m.price;

    auto it = lvl.orders.insert(lvl.orders.end(), Order{m.order_id, m.shares});
    lvl.totalQty += m.shares;
    index_[m.order_id] = Locator{m.side, m.price, it};
}

void LocalOrderBook::apply(const OrderExecuted& m)
{
    reduce(m.order_id, m.executed_shares);
}

void LocalOrderBook::apply(const OrderCancel& m)
{
    reduce(m.order_id, m.canceled_shares);
}

void LocalOrderBook::apply(const OrderDelete& m)
{
    auto found = index_.find(m.order_id);
    if (found == index_.end())
    {
        return;
    }

    const Locator loc = found->second;
    auto& side = sideMap(loc.side);
    auto levelIt = side.find(loc.price);
    if (levelIt != side.end())
    {
        Level& lvl = levelIt->second;
        lvl.totalQty -= loc.it->shares;
        lvl.orders.erase(loc.it);
        if (lvl.orders.empty())
        {
            side.erase(levelIt);
        }
    }
    index_.erase(found);
}

void LocalOrderBook::reduce(std::uint64_t id, std::uint32_t by)
{
    auto found = index_.find(id);
    if (found == index_.end())
    {
        return;
    }

    const Locator loc = found->second;
    auto& side = sideMap(loc.side);
    auto levelIt = side.find(loc.price);
    if (levelIt == side.end())
    {
        index_.erase(found);
        return;
    }

    Level& lvl = levelIt->second;
    const std::uint32_t dec = std::min(by, loc.it->shares);
    loc.it->shares -= dec;
    lvl.totalQty -= dec;

    if (loc.it->shares == 0)
    {
        lvl.orders.erase(loc.it);
        index_.erase(found);
        if (lvl.orders.empty())
        {
            side.erase(levelIt);
        }
    }
}

LocalOrderBook::L1 LocalOrderBook::top() const
{
    L1 out{};
    if (!bids_.empty())
    {
        auto it = std::prev(bids_.end());
        out.bidPrice = it->first;
        out.bidQty = it->second.totalQty;
    }
    if (!asks_.empty())
    {
        auto it = asks_.begin();
        out.askPrice = it->first;
        out.askQty = it->second.totalQty;
    }
    return out;
}

void LocalOrderBook::printL2(std::size_t depth) const
{
    std::cout << "----- book (" << index_.size() << " orders) -----\n";
    std::cout << "  asks:\n";
    std::size_t n = 0;
    for (auto it = asks_.begin(); it != asks_.end() && n < depth; ++it, ++n)
    {
        std::cout << "    " << it->first << " x " << it->second.totalQty
                  << "  (" << it->second.orders.size() << " orders)\n";
    }
    std::cout << "  bids:\n";
    n = 0;
    for (auto it = bids_.rbegin(); it != bids_.rend() && n < depth; ++it, ++n)
    {
        std::cout << "    " << it->first << " x " << it->second.totalQty
                  << "  (" << it->second.orders.size() << " orders)\n";
    }
}
