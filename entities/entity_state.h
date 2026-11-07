#pragma once

#include "../runtime/result.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tw::entities
{
    struct EntityKey final
    {
        std::uint32_t index{};
        std::uint32_t generation{};
        std::uint64_t epoch{};

        [[nodiscard]] friend constexpr bool operator==(const EntityKey&, const EntityKey&) noexcept = default;
    };

    class EntityRegistry final
    {
    public:
        explicit EntityRegistry(std::size_t maximumEntities = 32768);

        [[nodiscard]] runtime::Result<EntityKey> observeAdd(std::uint32_t index) noexcept;
        [[nodiscard]] runtime::Status observeRemove(EntityKey key) noexcept;
        [[nodiscard]] bool valid(EntityKey key) const noexcept;
        [[nodiscard]] std::uint64_t epoch() const noexcept;
        void advanceEpoch() noexcept;

    private:
        struct Slot final
        {
            std::uint32_t generation{};
            bool occupied{};
        };

        mutable std::mutex mutex_{};
        std::vector<Slot> slots_{};
        std::uint64_t epoch_{ 1 };
    };

    struct Vec3 final
    {
        float x{};
        float y{};
        float z{};
    };

    struct PlayerSnapshot final
    {
        EntityKey key{};
        std::string name{};
        Vec3 origin{};
        Vec3 eyePosition{};
        int team{};
        int health{};
        bool alive{};
        bool visible{};
    };

    struct WorldSnapshot final
    {
        std::uint64_t epoch{};
        std::uint64_t sequence{};
        double timeSeconds{};
        std::vector<PlayerSnapshot> players{};
    };

    class SnapshotPublication final
    {
    public:
        void publish(WorldSnapshot snapshot);
        [[nodiscard]] std::shared_ptr<const WorldSnapshot> current() const noexcept;
        void clear(std::uint64_t epoch);

    private:
        mutable std::mutex mutex_{};
        std::shared_ptr<const WorldSnapshot> current_{ std::make_shared<const WorldSnapshot>() };
    };
}
