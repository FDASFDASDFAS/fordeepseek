#pragma once

#include "../../commands/command_pipeline.h"
#include "../../entities/entity_state.h"
#include "../../runtime/result.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tw::features
{
    struct EntityKeyHash final
    {
        [[nodiscard]] std::size_t operator()(const entities::EntityKey& key) const noexcept;
    };

    template <typename Sample>
    class BoundedHistory final
    {
    public:
        BoundedHistory(std::size_t maximumPerEntity, double maximumAgeSeconds)
            : maximumPerEntity_(maximumPerEntity), maximumAgeSeconds_(maximumAgeSeconds) {}

        void push(entities::EntityKey key, double timeSeconds, Sample sample)
        {
            auto& samples = samples_[key];
            samples.push_front({ timeSeconds, std::move(sample) });
            while (samples.size() > maximumPerEntity_)
                samples.pop_back();
            while (!samples.empty() && timeSeconds - samples.back().timeSeconds > maximumAgeSeconds_)
                samples.pop_back();
        }

        [[nodiscard]] std::vector<Sample> get(entities::EntityKey key) const
        {
            std::vector<Sample> result;
            const auto iterator = samples_.find(key);
            if (iterator == samples_.end())
                return result;
            result.reserve(iterator->second.size());
            for (const auto& entry : iterator->second)
                result.push_back(entry.sample);
            return result;
        }

        void erase(entities::EntityKey key) { samples_.erase(key); }
        void clear() noexcept { samples_.clear(); }

    private:
        struct Entry final
        {
            double timeSeconds{};
            Sample sample{};
        };

        std::size_t maximumPerEntity_{};
        double maximumAgeSeconds_{};
        std::unordered_map<entities::EntityKey, std::deque<Entry>, EntityKeyHash> samples_{};
    };

    struct CandidateRank final
    {
        float score{};
        entities::EntityKey entity{};
        std::uint32_t sampleOrdinal{};
        std::uint32_t pointOrdinal{};
    };

    [[nodiscard]] bool candidateLess(const CandidateRank& left, const CandidateRank& right) noexcept;

    struct AimCandidate final
    {
        entities::EntityKey key{};
        entities::Vec3 eyePosition{};
        int team{};
    };

    // Current-snapshot-only FOV target selection: no history, penetration, or visibility trace yet.
    [[nodiscard]] std::optional<AimCandidate> selectAimTarget(
        entities::Vec3 localEyePosition, commands::Angles viewAngles, int localTeam,
        const std::vector<AimCandidate>& candidates, bool teamCheck, float fovDegrees) noexcept;

    enum class WorldEntityCategory : std::uint8_t { Unknown, Item, Grenade, Bomb };

    // Classifies a live entity by its own class name rather than an exhaustive hardcoded weapon/grenade
    // table -- current-runtime classification per the legacy-source-to-cs2 REBUILD rule. A class name
    // this build doesn't actually use simply classifies as Unknown (safe, visibly-empty failure), never
    // a fabricated guess standing in for an unverified fact.
    [[nodiscard]] WorldEntityCategory classifyWorldEntity(std::string_view className) noexcept;

    // Strips the "C_" prefix and a trailing "Projectile" suffix, then splits CamelCase into words, e.g.
    // "C_SmokeGrenadeProjectile" -> "Smoke Grenade", "C_PlantedC4" -> "Planted C4".
    [[nodiscard]] std::string worldEntityLabel(std::string_view className) noexcept;

    [[nodiscard]] runtime::Result<commands::Movement> correctMovement(
        float originalYaw, float finalYaw, commands::Movement movement) noexcept;
    // Relative scale, not an absolute magnitude cap: forward/side are CS2's raw wire-format move-speed
    // values (order of hundreds), so a fixed unit-space clamp would crush real movement almost to zero.
    // scale is a fraction of the player's own current input (0 = stopped, 1 = untouched).
    [[nodiscard]] commands::Movement scaleMovementSpeed(commands::Movement movement, float scale) noexcept;
    [[nodiscard]] bool bunnyHopJumpAllowed(bool jumpPressed, bool grounded, bool restrictedMoveType) noexcept;
    // Rotates the view toward the ideal circle-strafe angle for whichever side the player is already
    // holding; never overrides the player's own movement or button state. heldSide is the sign of the
    // player's own draft.movement.side (0 = not strafing). Fails (no intent should be pushed) when no
    // strafe direction is held or velocity is too low for a meaningful ideal angle.
	[[nodiscard]] runtime::Result<commands::Angles> airStrafeYaw(
		entities::Vec3 velocity, commands::Angles currentView, float heldSide) noexcept;
	// Converts the ideal world-space strafe direction into movement relative to the unchanged camera.
	// This keeps air-strafe independent from combat view intents and preserves the player's raw input magnitude.
	[[nodiscard]] runtime::Result<commands::Movement> airStrafeMovement(
		entities::Vec3 velocity, commands::Angles currentView, commands::Movement currentMovement,
		float heldSide) noexcept;
    [[nodiscard]] runtime::Result<commands::Angles> lookAngles(
        entities::Vec3 source, entities::Vec3 target) noexcept;
    [[nodiscard]] float angularDistance(commands::Angles current, commands::Angles target) noexcept;
    [[nodiscard]] runtime::Result<commands::Angles> smoothAim(
        commands::Angles current, commands::Angles target, float smoothing) noexcept;
    [[nodiscard]] runtime::Result<commands::Angles> compensateRecoilDelta(
        commands::Angles view, commands::Angles previousRecoil, commands::Angles currentRecoil,
        float pitchScale, float yawScale) noexcept;

    class WorkBudget final
    {
    public:
        explicit WorkBudget(std::size_t limit) noexcept : remaining_(limit) {}
        [[nodiscard]] bool consume(std::size_t amount = 1) noexcept;
        [[nodiscard]] std::size_t remaining() const noexcept { return remaining_; }

    private:
        std::size_t remaining_{};
    };

    class TriggerDwell final
    {
    public:
        [[nodiscard]] bool update(std::optional<entities::EntityKey> target, double nowSeconds, double delaySeconds) noexcept;
        void reset() noexcept;

    private:
        std::optional<entities::EntityKey> target_{};
        double acquiredAt_{};
    };

    enum class QuickPeekState : std::uint8_t { Idle, Anchored, Returning };

    class QuickPeekPolicy final
    {
    public:
        void activate(entities::Vec3 position) noexcept;
		void onCommittedShot() noexcept;
		void cancel() noexcept;
		[[nodiscard]] std::optional<commands::Movement> update(
			entities::Vec3 position, float viewYaw, float moveSpeed) noexcept;
        [[nodiscard]] QuickPeekState state() const noexcept { return state_; }

    private:
        QuickPeekState state_{ QuickPeekState::Idle };
        entities::Vec3 anchor_{};
    };

    struct TrajectorySample final
    {
        entities::Vec3 position{};
        entities::Vec3 velocity{};
        bool collided{};
    };

    using CollisionPolicy = std::function<TrajectorySample(const TrajectorySample&, float)>;

    [[nodiscard]] runtime::Result<std::vector<TrajectorySample>> integrateTrajectory(
        TrajectorySample initial, float stepSeconds, std::size_t maximumSteps, const CollisionPolicy& collisionPolicy);

    class ICosmeticTransactionAdapter
    {
    public:
        virtual ~ICosmeticTransactionAdapter() = default;
        [[nodiscard]] virtual runtime::Result<std::uint64_t> capture() noexcept = 0;
        [[nodiscard]] virtual runtime::Status apply(std::uint64_t opaqueSelection) noexcept = 0;
        [[nodiscard]] virtual runtime::Status restore(std::uint64_t opaqueCapture) noexcept = 0;
    };

    [[nodiscard]] runtime::Status applyCosmeticTransaction(
        ICosmeticTransactionAdapter& adapter, std::uint64_t opaqueSelection) noexcept;
}
