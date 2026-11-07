#include "shot_tracker.h"

#include <algorithm>
#include <cmath>

namespace tw::features
{
    std::uint64_t ShotTracker::registerShot(
        const std::uint64_t epoch, const entities::EntityKey shooter,
        const std::optional<entities::EntityKey> intendedTarget, const double timeSeconds)
    {
        if (capacity_ == 0 || !std::isfinite(timeSeconds) || shooter.epoch != epoch)
            return 0;
        while (records_.size() >= capacity_)
            records_.pop_front();
        const std::uint64_t id = nextId_++;
        records_.push_back({ id, epoch, shooter, intendedTarget, std::nullopt, timeSeconds });
        return id;
    }

    bool ShotTracker::observeImpact(
        const std::uint64_t epoch, const entities::EntityKey shooter, const double timeSeconds, const entities::Vec3 position)
    {
        ShotRecord* record = findMatch(epoch, shooter, timeSeconds, false);
        if (!record)
            return false;
        if (record->impacts.size() >= 4)
        {
            record->state = ShotState::Ambiguous;
            return false;
        }
        record->impacts.push_back({ timeSeconds, position });
        if (record->state == ShotState::Pending)
            record->state = ShotState::Impacted;
        return true;
    }

    bool ShotTracker::observeHurt(
        const std::uint64_t epoch, const entities::EntityKey attacker, const entities::EntityKey target, const double timeSeconds)
    {
        ShotRecord* record = findMatch(epoch, attacker, timeSeconds, false);
        if (!record)
            return false;
        if (record->intendedTarget && *record->intendedTarget != target)
            record->state = ShotState::Ambiguous;
        else
            record->state = ShotState::Hurt;
        record->hurtTarget = target;
        return true;
    }

    void ShotTracker::expire(const std::uint64_t epoch, const double nowSeconds) noexcept
    {
        for (auto& record : records_)
        {
            if (record.epoch != epoch || nowSeconds - record.firedAt > matchWindowSeconds_)
            {
                if (record.state == ShotState::Pending || record.state == ShotState::Impacted)
                    record.state = ShotState::Expired;
            }
        }
    }

    void ShotTracker::clear(const std::uint64_t epoch) noexcept
    {
        records_.erase(std::remove_if(records_.begin(), records_.end(), [epoch](const ShotRecord& record)
        {
            return record.epoch != epoch;
        }), records_.end());
    }

    std::vector<ShotRecord> ShotTracker::records() const
    {
        return { records_.begin(), records_.end() };
    }

    ShotRecord* ShotTracker::findMatch(
        const std::uint64_t epoch, const entities::EntityKey shooter, const double timeSeconds, const bool requireImpact) noexcept
    {
        ShotRecord* match{};
        for (auto iterator = records_.rbegin(); iterator != records_.rend(); ++iterator)
        {
            if (iterator->epoch != epoch || iterator->shooter != shooter || timeSeconds < iterator->firedAt ||
                timeSeconds - iterator->firedAt > matchWindowSeconds_ || iterator->state == ShotState::Expired ||
                iterator->state == ShotState::Hurt || (requireImpact && iterator->impacts.empty()))
                continue;
            if (match != nullptr)
            {
                match->state = ShotState::Ambiguous;
                iterator->state = ShotState::Ambiguous;
                return nullptr;
            }
            match = &*iterator;
        }
        return match;
    }
}
