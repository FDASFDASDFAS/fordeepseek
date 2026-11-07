#pragma once

#include "../../entities/entity_state.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace tw::features
{
    enum class ShotState : std::uint8_t
    {
        Pending,
        Impacted,
        Hurt,
        Ambiguous,
        Expired
    };

    struct ShotImpact final
    {
        double timeSeconds{};
        entities::Vec3 position{};
    };

    struct ShotRecord final
    {
        std::uint64_t id{};
        std::uint64_t epoch{};
        entities::EntityKey shooter{};
        std::optional<entities::EntityKey> intendedTarget{};
        std::optional<entities::EntityKey> hurtTarget{};
        double firedAt{};
        ShotState state{ ShotState::Pending };
        std::vector<ShotImpact> impacts{};
    };

    class ShotTracker final
    {
    public:
        explicit ShotTracker(std::size_t capacity = 32, double matchWindowSeconds = 1.0) noexcept
            : capacity_(capacity), matchWindowSeconds_(matchWindowSeconds) {}

        [[nodiscard]] std::uint64_t registerShot(
            std::uint64_t epoch, entities::EntityKey shooter, std::optional<entities::EntityKey> intendedTarget,
            double timeSeconds);
        [[nodiscard]] bool observeImpact(
            std::uint64_t epoch, entities::EntityKey shooter, double timeSeconds, entities::Vec3 position);
        [[nodiscard]] bool observeHurt(
            std::uint64_t epoch, entities::EntityKey attacker, entities::EntityKey target, double timeSeconds);
        void expire(std::uint64_t epoch, double nowSeconds) noexcept;
        void clear(std::uint64_t epoch) noexcept;
        [[nodiscard]] std::vector<ShotRecord> records() const;

    private:
        [[nodiscard]] ShotRecord* findMatch(
            std::uint64_t epoch, entities::EntityKey shooter, double timeSeconds, bool requireImpact) noexcept;

        std::size_t capacity_{};
        double matchWindowSeconds_{};
        std::uint64_t nextId_{ 1 };
        std::deque<ShotRecord> records_{};
    };
}
