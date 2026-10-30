#include "../native/async-owner-registry.h"
#include <functional>
#include <iostream>
#include <new>
#include <vector>

void expect(bool good,const char* message) {
    if (!good) throw std::runtime_error(message);
}
struct Owner { unsigned deliveries=0; };
static_assert(!std::is_copy_constructible<wxlambda::AsyncOwnerRegistry<Owner>>::value,
              "Cancellation registries cannot duplicate an identity namespace");
static_assert(!std::is_move_constructible<wxlambda::AsyncOwnerRegistry<Owner>>::value,
              "Registry address must remain stable for queued completion lookup");
int main() {
    try {
        wxlambda::AsyncOwnerRegistry<Owner> owners;
        alignas(Owner) unsigned char storage[sizeof(Owner)];
        auto* first=new(storage) Owner;
        const auto original=owners.insert(first);
        std::vector<std::function<void()>> completions;
        completions.emplace_back([&owners,original] {
            if (auto* current=owners.find(original)) ++current->deliveries;
        });
        expect(first->deliveries==0,"callback must wait for explicit delivery");
        completions.front()();
        expect(first->deliveries==1,"live callback resolves its owner");
        owners.erase(original); first->~Owner();
        auto* replacement=new(storage) Owner;
        const auto fresh=owners.insert(replacement);
        completions.front()();
        expect(fresh!=original && replacement->deliveries==0,"pointer reuse must not revive old completion");
        owners.erase(fresh); replacement->~Owner();
        completions.front()();
        expect(!owners.find(original) && owners.size()==0,"late/session shutdown delivery is inert");

        // Model the environment -> controller gap: owner retires between stages.
        Owner staged;
        const auto staged_id=owners.insert(&staged);
        bool controller_closed=false;
        const auto controller_ready=[&owners,staged_id,&controller_closed] {
            if (auto* current=owners.find(staged_id)) ++current->deliveries;
            else controller_closed=true;
        };
        owners.erase(staged_id);
        controller_ready();
        expect(controller_closed && staged.deliveries==0,"late unadopted controller must close without owner access");

        // Exhaustion is deterministic and fails closed, even after cancellation.
        wxlambda::AsyncOwnerRegistry<Owner,std::uint8_t> small;
        Owner owner;
        for (unsigned i=1;i<=255;++i) {
            const auto id=small.insert(&owner);
            expect(id==i,"identities must increase without reuse"); small.erase(id);
        }
        bool exhausted=false;
        try { small.insert(&owner); } catch (const std::overflow_error&) { exhausted=true; }
        expect(exhausted,"exhaustion cannot wrap to an old identity");
        bool null_rejected=false;
        try { owners.insert(nullptr); } catch (const std::invalid_argument&) { null_rejected=true; }
        expect(null_rejected,"null owner rejects");
        std::cout << "PASS: numeric asynchronous owner retirement, address reuse and exhaustion\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
