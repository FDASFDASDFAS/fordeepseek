#pragma once

#include "../../entities/entity_state.h"
#include "../../renderer/render_commands.h"
#include "../../utils/math/vector/vector.h"
#include "../../utils/math/viewmatrix/viewmatrix.h"
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace tw::features
{
    struct HitRecord final
    {
        double timeSeconds{};
        std::string targetName{};
        int damage{};
        int remainingHealth{};
        bool isKill{};
        entities::Vec3 impactPosition{};
    };

    struct TracerRecord final
    {
        double timeSeconds{};
        entities::Vec3 startPosition{};
        entities::Vec3 endPosition{};
        bool isHit{};
    };

    struct ImpactRecord final
    {
        double timeSeconds{};
        entities::Vec3 position{};
        bool isHit{};
    };

    class HitFeedback final
    {
    public:
        void update(double nowSeconds, float screenWidth, float screenHeight);
        void onEnemyDamaged(const std::string& name, int damage, int remainingHealth,
            const entities::Vec3& localEye, const entities::Vec3& enemyPos, double nowSeconds);
        void onShotFired(const entities::Vec3& localEye, const entities::Vec3& viewForward, double nowSeconds);

        void renderOverlay(std::vector<tw::renderer::DrawCommand>& commands,
            const viewmatrix_t& viewMatrix, float screenWidth, float screenHeight, double nowSeconds);

        void clear() noexcept;
        static void initHitSounds();

        // Driven by the player_hurt game event, which carries the authoritative
        // attacker and hitgroup. Polled health deltas cannot tell who caused the
        // damage, so they fired on teammate damage, fall damage and kills made
        // by other players.
        void onLocalPlayerHit(const std::string& victimName, int damage, int remainingHealth,
            int hitgroup, const entities::Vec3& localEye, const entities::Vec3& victimPos,
            bool positionValid, double nowSeconds);

        static constexpr int kHitSoundCount = 47;
        static inline const char* kHitSoundNames[] = {
            "agpa1", "agpa2", "aimbooster", "amongus_kill", "applepay",
            "arena_switch", "bameware", "bell", "ben", "bonk",
            "brotato", "bubble", "click", "cod", "combobreak",
            "door", "fatality", "flush", "hentai1", "hentai2",
            "hentai3", "kick", "kill_doof_01", "killcard_1", "minecraft_bow_ding",
            "minecraft_button", "minecraft_egg_throw", "minecraft_hit", "minecraft_xp_gain", "money_claim",
            "mouthsound", "msfrs", "na naxuy", "pop", "pubg_pan",
            "quaver", "regulus", "rust_headshot", "satisfying click", "spiral knight",
            "stony", "tavern_misc1c", "telegram_notification", "trident_pierce", "water_drop",
            "xp_rankdown_02", "zelda"
        };

    private:
        std::deque<HitRecord> hits_{};
        std::deque<TracerRecord> tracers_{};
        std::deque<ImpactRecord> impacts_{};
        double lastHitTime_{ -10.0 };
        bool lastHitWasKill_{};
    };

    extern HitFeedback g_hitFeedback;
}