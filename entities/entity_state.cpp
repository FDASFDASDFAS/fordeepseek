#include "entity_state.h"

#include <limits>
#include <utility>

namespace tw::entities
{
    EntityRegistry::EntityRegistry(const std::size_t maximumEntities) : slots_(maximumEntities) {}

    runtime::Result<EntityKey> EntityRegistry::observeAdd(const std::uint32_t index) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        if (index >= slots_.size())
            return runtime::Result<EntityKey>::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Entities, 1, static_cast<long>(index) });
        auto& slot = slots_[index];
        if (slot.occupied)
            return runtime::Result<EntityKey>::failure({ runtime::StatusCode::InvalidState, runtime::Subsystem::Entities, 2, static_cast<long>(index) });
        if (slot.generation == std::numeric_limits<std::uint32_t>::max())
            slot.generation = 1;
        else
            ++slot.generation;
        slot.occupied = true;
        return runtime::Result<EntityKey>::success({ index, slot.generation, epoch_ });
    }

    runtime::Status EntityRegistry::observeRemove(const EntityKey key) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        if (key.index >= slots_.size() || key.epoch != epoch_)
            return runtime::Status::failure({ runtime::StatusCode::StaleIdentity, runtime::Subsystem::Entities, 3, 0 });
        auto& slot = slots_[key.index];
        if (!slot.occupied || slot.generation != key.generation)
            return runtime::Status::failure({ runtime::StatusCode::StaleIdentity, runtime::Subsystem::Entities, 4, 0 });
        slot.occupied = false;
        return runtime::Status::success();
    }

    bool EntityRegistry::valid(const EntityKey key) const noexcept
    {
        std::scoped_lock lock{ mutex_ };
        return key.index < slots_.size() && key.epoch == epoch_ && slots_[key.index].occupied &&
            slots_[key.index].generation == key.generation;
    }

    std::uint64_t EntityRegistry::epoch() const noexcept
    {
        std::scoped_lock lock{ mutex_ };
        return epoch_;
    }

    void EntityRegistry::advanceEpoch() noexcept
    {
        std::scoped_lock lock{ mutex_ };
        ++epoch_;
        for (auto& slot : slots_)
            slot.occupied = false;
    }

    void SnapshotPublication::publish(WorldSnapshot snapshot)
    {
        auto complete = std::make_shared<const WorldSnapshot>(std::move(snapshot));
        std::scoped_lock lock{ mutex_ };
        current_ = std::move(complete);
    }

    std::shared_ptr<const WorldSnapshot> SnapshotPublication::current() const noexcept
    {
        std::scoped_lock lock{ mutex_ };
        return current_;
    }

    void SnapshotPublication::clear(const std::uint64_t epoch)
    {
        publish(WorldSnapshot{ epoch });
    }
}
