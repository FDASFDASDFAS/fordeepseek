#pragma once

#include "../../entities/entity_state.h"

#include <cstddef>

namespace tw::smokes
{
    // Active smoke volumes, so the legit aim can refuse a target the bullet
    // would have to cross a smoke to reach.
    //
    // The occlusion test is geometric, not the engine's own. CS2 stores the
    // real occupancy in C_SmokeGrenadeProjectile::m_VoxelFrameData, a voxel
    // grid whose encoding this project has not reverse engineered, and there is
    // no resolved LineGoesThroughSmoke export to call instead. So the volume is
    // modelled from the shape the game's own convars describe -- see smokes.cpp
    // for exactly which numbers are read off the build and which are inferred.

    // Resolves the C_SmokeGrenadeProjectile schema fields. Returns false when
    // any of them is missing, in which case every query below reports "no
    // smoke" rather than guessing -- a missing schema must not silently start
    // refusing every shot.
    bool init();

    [[nodiscard]] bool available() noexcept;

    // Number of smokes currently detonated and networked to us. Diagnostic
    // only; the aim path does not need it.
    [[nodiscard]] std::size_t activeCount() noexcept;

    // True when the segment start->end passes through an active smoke volume.
    // Always false when init() failed or no smoke is live, so a caller that
    // gates on it degrades to its previous behaviour.
    [[nodiscard]] bool lineGoesThroughSmoke(
        const entities::Vec3& start, const entities::Vec3& end) noexcept;
}
