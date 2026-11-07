#include "feature_policies.h"

#include <cctype>
#include <cmath>
#include <limits>
#include <tuple>

namespace tw::features
{
    std::size_t EntityKeyHash::operator()(const entities::EntityKey& key) const noexcept
    {
        const auto first = static_cast<std::size_t>(key.epoch ^ (key.epoch >> 32));
        return first ^ (static_cast<std::size_t>(key.index) << 1) ^ (static_cast<std::size_t>(key.generation) << 17);
    }

    bool candidateLess(const CandidateRank& left, const CandidateRank& right) noexcept
    {
        if (left.score != right.score)
            return left.score > right.score;
        return std::tie(left.entity.epoch, left.entity.index, left.entity.generation, left.sampleOrdinal, left.pointOrdinal) <
            std::tie(right.entity.epoch, right.entity.index, right.entity.generation, right.sampleOrdinal, right.pointOrdinal);
    }

    std::optional<AimCandidate> selectAimTarget(
        const entities::Vec3 localEyePosition, const commands::Angles viewAngles, const int localTeam,
        const std::vector<AimCandidate>& candidates, const bool teamCheck, const float fovDegrees) noexcept
    {
        if (!std::isfinite(fovDegrees) || fovDegrees <= 0.0f)
            return std::nullopt;

        bool found{};
        CandidateRank best{};
        std::optional<AimCandidate> bestCandidate{};
        for (const auto& candidate : candidates)
        {
			if (teamCheck && candidate.team == localTeam)
                continue;
            const auto look = lookAngles(localEyePosition, candidate.eyePosition);
            if (!look)
                continue;
            const float distance = angularDistance(viewAngles, look.value());
            if (!std::isfinite(distance) || distance > fovDegrees)
                continue;
            const CandidateRank rank{ -distance, candidate.key, 0, 0 };
            if (!found || candidateLess(rank, best))
            {
                best = rank;
                bestCandidate = candidate;
                found = true;
            }
        }
        return bestCandidate;
    }

    runtime::Result<commands::Movement> correctMovement(
        const float originalYaw, const float finalYaw, const commands::Movement movement) noexcept
    {
        if (!std::isfinite(originalYaw) || !std::isfinite(finalYaw) || !std::isfinite(movement.forward) ||
            !std::isfinite(movement.side) || !std::isfinite(movement.up))
            return runtime::Result<commands::Movement>::failure(
                { runtime::StatusCode::InvalidArgument, runtime::Subsystem::Features, 4, 0 });
        constexpr float radiansPerDegree = 0.01745329251994329577f;
        const float delta = (finalYaw - originalYaw) * radiansPerDegree;
        const float cosine = std::cos(delta);
        const float sine = std::sin(delta);
        return runtime::Result<commands::Movement>::success({
            cosine * movement.forward + sine * movement.side,
            -sine * movement.forward + cosine * movement.side,
            movement.up
        });
    }

    commands::Movement scaleMovementSpeed(commands::Movement movement, const float scale) noexcept
    {
        if (!std::isfinite(scale) || scale < 0.0f || !std::isfinite(movement.forward) || !std::isfinite(movement.side))
            return {};
        movement.forward *= scale;
        movement.side *= scale;
        return movement;
    }

    bool bunnyHopJumpAllowed(const bool jumpPressed, const bool grounded, const bool restrictedMoveType) noexcept
    {
        return jumpPressed && grounded && !restrictedMoveType;
    }

	runtime::Result<commands::Angles> airStrafeYaw(
        const entities::Vec3 velocity, const commands::Angles currentView, const float heldSide) noexcept
    {
        if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y) || !std::isfinite(velocity.z) ||
            !std::isfinite(currentView.pitch) || !std::isfinite(currentView.yaw) || !std::isfinite(currentView.roll) ||
            !std::isfinite(heldSide))
            return runtime::Result<commands::Angles>::failure(
                { runtime::StatusCode::InvalidArgument, runtime::Subsystem::Features, 5, 0 });
        if (heldSide == 0.0f)
            return runtime::Result<commands::Angles>::failure(
                { runtime::StatusCode::Unsupported, runtime::Subsystem::Features, 6, 0 });
        const float speed = std::hypot(velocity.x, velocity.y);
        if (speed < 5.0f)
            return runtime::Result<commands::Angles>::failure(
                { runtime::StatusCode::Unsupported, runtime::Subsystem::Features, 7, 0 });

        constexpr float degreesPerRadian = 57.29577951308232f;
        const float velocityYaw = std::atan2(velocity.y, velocity.x) * degreesPerRadian;
        const float ratio = std::clamp(30.0f / speed, -1.0f, 1.0f);
        const float idealOffset = std::clamp(std::asin(ratio) * degreesPerRadian, 5.0f, 45.0f);
        const float targetYaw = velocityYaw + (heldSide > 0.0f ? idealOffset : -idealOffset);
		return runtime::Result<commands::Angles>::success({
			currentView.pitch, std::remainder(targetYaw, 360.0f), 0.0f
		});
	}

	runtime::Result<commands::Movement> airStrafeMovement(
		const entities::Vec3 velocity, const commands::Angles currentView,
		const commands::Movement currentMovement, const float heldSide) noexcept
	{
		if (!std::isfinite(currentMovement.forward) || !std::isfinite(currentMovement.side) ||
			!std::isfinite(currentMovement.up))
			return runtime::Result<commands::Movement>::failure(
				{ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Features, 8, 0 });

		const auto target = airStrafeYaw(velocity, currentView, heldSide);
		const float magnitude = std::hypot(currentMovement.forward, currentMovement.side);
		if (!target)
			return runtime::Result<commands::Movement>::failure(target.error());
		if (magnitude <= 0.001f)
			return runtime::Result<commands::Movement>::failure(
				{ runtime::StatusCode::Unsupported, runtime::Subsystem::Features, 9, 0 });

		constexpr float radiansPerDegree = 0.01745329251994329577f;
		const float delta = std::remainder(target.value().yaw - currentView.yaw, 360.0f) * radiansPerDegree;
		return runtime::Result<commands::Movement>::success({
			std::cos(delta) * magnitude,
			std::sin(delta) * magnitude,
			currentMovement.up
		});
	}

    runtime::Result<commands::Angles> lookAngles(
        const entities::Vec3 source, const entities::Vec3 target) noexcept
    {
        const float deltaX = target.x - source.x;
        const float deltaY = target.y - source.y;
        const float deltaZ = target.z - source.z;
        const float distance = std::hypot(deltaX, deltaY);

        if (distance <= 0.001f)
            return runtime::Result<commands::Angles>::failure(
                { runtime::StatusCode::InvalidArgument, runtime::Subsystem::Features, 10, 0 });

        constexpr float degreesPerRadian = 57.29577951308232f;
        const float pitch = std::atan2(-deltaZ, distance) * degreesPerRadian;
        const float yaw = std::atan2(deltaY, deltaX) * degreesPerRadian;

        return runtime::Result<commands::Angles>::success({
            std::clamp(pitch, -89.0f, 89.0f),
            std::remainder(yaw, 360.0f),
            0.0f
        });
    }

    float angularDistance(const commands::Angles current, const commands::Angles target) noexcept
    {
        const float pitchDelta = std::remainder(target.pitch - current.pitch, 360.0f);
        const float yawDelta = std::remainder(target.yaw - current.yaw, 360.0f);
        return std::hypot(pitchDelta, yawDelta);
    }

    runtime::Result<commands::Angles> smoothAim(
        const commands::Angles current, const commands::Angles target, const float smoothing) noexcept
    {
        if (!std::isfinite(smoothing) || smoothing <= 0.0f)
            return runtime::Result<commands::Angles>::success(target);

        const float pitchDelta = std::remainder(target.pitch - current.pitch, 360.0f);
        const float yawDelta = std::remainder(target.yaw - current.yaw, 360.0f);
        const float smoothFactor = std::max(1.0f, smoothing);

        return runtime::Result<commands::Angles>::success({
            std::clamp(current.pitch + (pitchDelta / smoothFactor), -89.0f, 89.0f),
            std::remainder(current.yaw + (yawDelta / smoothFactor), 360.0f),
            0.0f
        });
    }

    runtime::Result<commands::Angles> compensateRecoilDelta(
        const commands::Angles view, const commands::Angles previousRecoil, const commands::Angles currentRecoil,
        const float pitchScale, const float yawScale) noexcept
    {
        if (!std::isfinite(pitchScale) || !std::isfinite(yawScale))
            return runtime::Result<commands::Angles>::failure(
                { runtime::StatusCode::InvalidArgument, runtime::Subsystem::Features, 12, 0 });

        const float deltaPitch = (currentRecoil.pitch - previousRecoil.pitch) * pitchScale;
        const float deltaYaw = (currentRecoil.yaw - previousRecoil.yaw) * yawScale;

        return runtime::Result<commands::Angles>::success({
            std::clamp(view.pitch - deltaPitch, -89.0f, 89.0f),
            std::remainder(view.yaw - deltaYaw, 360.0f),
            view.roll
        });
    }

    bool WorkBudget::consume(const std::size_t amount) noexcept
    {
        if (amount > remaining_)
            return false;
        remaining_ -= amount;
        return true;
    }

    bool TriggerDwell::update(const std::optional<entities::EntityKey> target, const double nowSeconds, const double delaySeconds) noexcept
    {
        if (!target)
        {
            reset();
            return false;
        }
        if (!target_ || *target_ != *target)
        {
            target_ = target;
            acquiredAt_ = nowSeconds;
            return delaySeconds <= 0.0;
        }
        return nowSeconds - acquiredAt_ >= std::max(0.0, delaySeconds);
    }

    void TriggerDwell::reset() noexcept
    {
        target_.reset();
        acquiredAt_ = 0.0;
    }

    void QuickPeekPolicy::activate(const entities::Vec3 position) noexcept
    {
        if (state_ == QuickPeekState::Idle)
        {
            anchor_ = position;
            state_ = QuickPeekState::Anchored;
        }
    }

    void QuickPeekPolicy::onCommittedShot() noexcept
    {
        if (state_ == QuickPeekState::Anchored)
            state_ = QuickPeekState::Returning;
    }

    void QuickPeekPolicy::cancel() noexcept
    {
        state_ = QuickPeekState::Idle;
    }

	std::optional<commands::Movement> QuickPeekPolicy::update(
		const entities::Vec3 position, const float viewYaw, const float moveSpeed) noexcept
	{
		if (state_ != QuickPeekState::Returning)
			return std::nullopt;

        const float deltaX = anchor_.x - position.x;
        const float deltaY = anchor_.y - position.y;
        const float distance = std::hypot(deltaX, deltaY);
		if (!std::isfinite(distance) || !std::isfinite(viewYaw) || !std::isfinite(moveSpeed) ||
			moveSpeed <= 0.0f || distance <= 5.0f)
		{
			cancel();
			return std::nullopt;
		}
		constexpr float degreesPerRadian = 57.29577951308232f;
		constexpr float radiansPerDegree = 0.01745329251994329577f;
		const float worldYaw = std::atan2(deltaY, deltaX) * degreesPerRadian;
		const float relativeYaw = (worldYaw - viewYaw) * radiansPerDegree;
		return commands::Movement{ std::cos(relativeYaw) * moveSpeed,
			std::sin(relativeYaw) * moveSpeed, 0.0f };
	}

    runtime::Result<std::vector<TrajectorySample>> integrateTrajectory(
        TrajectorySample initial, const float stepSeconds, const std::size_t maximumSteps, const CollisionPolicy& collisionPolicy)
    {
        if (!collisionPolicy || !std::isfinite(stepSeconds) || stepSeconds <= 0.0f || maximumSteps == 0 || maximumSteps > 4096)
            return runtime::Result<std::vector<TrajectorySample>>::failure(
                { runtime::StatusCode::InvalidArgument, runtime::Subsystem::Features, 1, 0 });

        std::vector<TrajectorySample> result;
        result.reserve(maximumSteps + 1);
        result.push_back(initial);
        for (std::size_t step = 0; step < maximumSteps; ++step)
        {
            initial = collisionPolicy(initial, stepSeconds);
            const auto finite = [](const entities::Vec3& value)
            {
                return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
            };
            if (!finite(initial.position) || !finite(initial.velocity))
                return runtime::Result<std::vector<TrajectorySample>>::failure(
                    { runtime::StatusCode::RuntimeFailure, runtime::Subsystem::Features, 2, static_cast<long>(step) });
            result.push_back(initial);
        }
        return runtime::Result<std::vector<TrajectorySample>>::success(std::move(result));
    }

    namespace
    {
        // Verified live CS2 class names, not a guessed naming convention: most weapons are "C_Weapon*"
        // but several (C_DEagle, C_AK47) are not, and ground/inventory grenades (C_HEGrenade) use a
        // different class than their in-flight thrown counterpart (C_HEGrenadeProjectile).
        constexpr std::string_view kItemClasses[]{
            "C_DEagle", "C_WeaponElite", "C_WeaponFiveSeven", "C_WeaponGlock", "C_WeaponHKP2000",
            "C_WeaponUSPSilencer", "C_WeaponP250", "C_WeaponCZ75a", "C_WeaponTec9", "C_WeaponRevolver",
            "C_WeaponMAC10", "C_WeaponMP5SD", "C_WeaponMP7", "C_WeaponMP9", "C_WeaponBizon", "C_WeaponP90",
            "C_WeaponUMP45", "C_AK47", "C_WeaponM4A1", "C_WeaponM4A1Silencer", "C_WeaponAug", "C_WeaponFamas",
            "C_WeaponGalilAR", "C_WeaponSG556", "C_WeaponNOVA", "C_WeaponSawedoff", "C_WeaponXM1014",
            "C_WeaponMag7", "C_WeaponAWP", "C_WeaponG3SG1", "C_WeaponSCAR20", "C_WeaponSSG08", "C_WeaponM249",
            "C_WeaponNegev", "C_HEGrenade", "C_Flashbang", "C_SmokeGrenade", "C_MolotovGrenade",
            "C_IncendiaryGrenade", "C_DecoyGrenade", "C_C4", "C_WeaponTaser", "C_Item_Healthshot", "C_Knife",
        };

        constexpr std::string_view kGrenadeProjectileClasses[]{
            "C_HEGrenadeProjectile", "C_FlashbangProjectile", "C_SmokeGrenadeProjectile", "C_MolotovProjectile",
            "C_DecoyProjectile",
        };

        bool containsClass(const std::string_view* first, const std::string_view* last, const std::string_view value) noexcept
        {
            for (; first != last; ++first)
            {
                if (*first == value)
                    return true;
            }
            return false;
        }
    }

    WorldEntityCategory classifyWorldEntity(const std::string_view className) noexcept
    {
        if (containsClass(std::begin(kItemClasses), std::end(kItemClasses), className))
            return WorldEntityCategory::Item;
        if (className == "C_PlantedC4")
            return WorldEntityCategory::Bomb;
        if (containsClass(std::begin(kGrenadeProjectileClasses), std::end(kGrenadeProjectileClasses), className))
            return WorldEntityCategory::Grenade;
        // Fall back to the naming convention for anything not in the verified lists above (future or
        // unlisted weapons), rather than reporting Unknown for every CS2 update this build predates.
        if (className.rfind("C_Weapon", 0) == 0)
            return WorldEntityCategory::Item;
        if (className.find("Projectile") != std::string_view::npos)
            return WorldEntityCategory::Grenade;
        return WorldEntityCategory::Unknown;
    }

    std::string worldEntityLabel(std::string_view className) noexcept
    {
        if (className.size() > 2 && className[0] == 'C' && className[1] == '_')
            className.remove_prefix(2);
        constexpr std::string_view suffix = "Projectile";
        if (className.size() > suffix.size() && className.substr(className.size() - suffix.size()) == suffix)
            className.remove_suffix(suffix.size());

        std::string label;
        label.reserve(className.size() + 4);
        for (std::size_t index = 0; index < className.size(); ++index)
        {
            const char character = className[index];
            if (index > 0 && std::isupper(static_cast<unsigned char>(character)))
            {
                const bool previousUpper = std::isupper(static_cast<unsigned char>(className[index - 1]));
                const bool nextLower = index + 1 < className.size() &&
                    std::islower(static_cast<unsigned char>(className[index + 1]));
                // Word boundary either from lowercase into uppercase ("Smoke|Grenade"), or from an
                // acronym run into a new capitalized word ("HE|Grenade", but not "A|K47" mid-acronym).
                if (!previousUpper || nextLower)
                    label.push_back(' ');
            }
            label.push_back(character);
        }
        return label;
    }

    runtime::Status applyCosmeticTransaction(
        ICosmeticTransactionAdapter& adapter, const std::uint64_t opaqueSelection) noexcept
    {
        auto captured = adapter.capture();
        if (!captured)
            return runtime::Status::failure(captured.error());
        const auto applied = adapter.apply(opaqueSelection);
        if (applied)
            return applied;
        const auto restored = adapter.restore(captured.value());
        if (!restored)
            return runtime::Status::failure(
                { runtime::StatusCode::UnsafeTeardown, runtime::Subsystem::Features, 3, restored.error().nativeCode });
        return applied;
    }
}
