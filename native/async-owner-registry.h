#pragma once
#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <type_traits>

namespace wxlambda {
// Main-thread-only cancellation index, not ownership. Never capture an Owner*
// in asynchronous completions: capture its ticket and look it up on delivery.
// Erase before teardown. Reusing the address cannot resurrect a retired ticket.
template<class Owner, class Ticket=std::uint64_t> class AsyncOwnerRegistry {
    static_assert(std::is_integral<Ticket>::value && std::is_unsigned<Ticket>::value,
                  "Asynchronous identities require an unsigned integer type");
    std::unordered_map<Ticket,Owner*> owners;
    Ticket next=1;
public:
    AsyncOwnerRegistry() = default;
    AsyncOwnerRegistry(const AsyncOwnerRegistry&) = delete;
    AsyncOwnerRegistry& operator=(const AsyncOwnerRegistry&) = delete;
    AsyncOwnerRegistry(AsyncOwnerRegistry&&) = delete;
    AsyncOwnerRegistry& operator=(AsyncOwnerRegistry&&) = delete;
    Ticket insert(Owner* owner) {
        if (!owner) throw std::invalid_argument("Null asynchronous owner");
        if (!next) throw std::overflow_error("Asynchronous owner identities exhausted");
        const Ticket ticket=next;
        owners.emplace(ticket,owner); // Allocation failure does not consume a ticket.
        next=ticket==(std::numeric_limits<Ticket>::max)() ? 0 : Ticket(ticket+1);
        return ticket;
    }
    Owner* find(Ticket ticket) const noexcept {
        const auto found=owners.find(ticket);
        return found==owners.end() ? nullptr : found->second;
    }
    void erase(Ticket ticket) noexcept { owners.erase(ticket); }
    std::size_t size() const noexcept { return owners.size(); }
};
}
