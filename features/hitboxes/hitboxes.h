#pragma once

#include "../../utils/math/vector/vector.h"

#include <cstdint>

namespace tw::hitboxes
{
    // Hitgroup ids as the game reports them (CHitBox::m_nGroupId, and the same
    // values the player_hurt event carries) -- so a hitbox group can be compared
    // directly against the hitgroup already plumbed through onLocalPlayerHit.
    enum Group : int
    {
        kInvalid  = -1,
        kGeneric  = 0,
        kHead     = 1,
        kChest    = 2,
        kStomach  = 3,
        kLeftArm  = 4,
        kRightArm = 5,
        kLeftLeg  = 6,
        kRightLeg = 7,
        kNeck     = 8,
    };

    // Geometry of a single hitbox, read straight off the CHitBox a trace
    // reported. Positions are model-local; add the owning bone's transform to
    // put them in world space.
    struct Hitbox
    {
        Vector_t     mins{};
        Vector_t     maxs{};
        float        radius{};
        int          group{ kInvalid };
        int          index{};
        std::uint8_t shapeType{};
        bool         translationOnly{};
        const char*  name{ nullptr };
        const char*  boneName{ nullptr };
    };

    // Resolves the CHitBox field offsets from animationsystem.dll's schema.
    bool init();
    [[nodiscard]] bool available() noexcept;

    // Hitgroup of the CHitBox a trace landed on, or kInvalid when the pointer is
    // null or unreadable (the ray hit world geometry rather than a player).
    // This is the game's own classification -- no model walking involved.
    [[nodiscard]] int groupOf(std::uintptr_t hitboxData) noexcept;

    // Full geometry for a hitbox the trace reported. Returns false when the
    // pointer does not read as a plausible CHitBox.
    [[nodiscard]] bool describe(std::uintptr_t hitboxData, Hitbox& out) noexcept;

    [[nodiscard]] const char* groupName(int group) noexcept;
}
