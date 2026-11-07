#include "smokes.h"

#include "../../utils/schema/schema.h"
#include "../../utils/fnv1a/fnv1a.h"
#include "../../../cs2/entity/C_EntitySystem/entity.h"

#include <Windows.h>

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace tw::smokes
{
    namespace
    {
        // ------------------------------------------------------------------
        // Schema fields, all in client.dll's scope on C_SmokeGrenadeProjectile.
        // Resolved at runtime; there is deliberately no fallback literal, since
        // a wrong offset here would read garbage and start refusing shots for
        // no visible reason. Missing schema => feature reports unavailable.
        // ------------------------------------------------------------------
        struct Offsets
        {
            std::uint32_t tickBegin{};        // m_nSmokeEffectTickBegin, int32
            std::uint32_t didEffect{};        // m_bDidSmokeEffect, bool
            std::uint32_t detonationPos{};    // m_vSmokeDetonationPos, VectorWS
            bool          resolved{};
        };

        Offsets g_off{};

        // ------------------------------------------------------------------
        // Volume shape.
        //
        // The exact occupancy lives in m_VoxelFrameData, whose encoding is not
        // reverse engineered here, so the volume is modelled analytically.
        //
        // The three numbers below are this build's shipped defaults for the
        // convars that name the smoke's own geometry:
        //
        //   cl_smoke_origin_height        68   (float, replicated)
        //   cl_smoke_torus_ring_radius    61   (float, replicated)
        //   cl_smoke_torus_ring_subradius 88   (float, replicated)
        //
        // What is read off the build: the values, and the fact that the game
        // describes the cloud as a torus with a ring radius and a tube
        // subradius. What is inferred: that the ring axis is vertical and that
        // the origin height offsets the volume centre up from the detonation
        // position. Both follow from the names rather than from disassembly.
        //
        // The resulting solid is a spindle torus (subradius > ring radius, so
        // the hole is closed): widest at the centre height, 61 + 88 = 149 units
        // out, and roughly 63 units tall on the axis. That matches the
        // ~144-unit, wider-than-tall cloud the game actually draws, which is
        // the sanity check this model rests on.
        //
        // These are replicated convars, so a server can change them. They are
        // compiled in rather than read at runtime because the convar lookup is
        // a live crash source during map load and caching a null on the first
        // early call would poison it permanently. Re-read them here if a build
        // changes the defaults.
        // ------------------------------------------------------------------
        constexpr float kOriginHeight  = 68.0f;
        constexpr float kRingRadius    = 61.0f;
        constexpr float kRingSubRadius = 88.0f;

        // Bounding sphere of the torus about the volume centre.
        constexpr float kOuterRadius = kRingRadius + kRingSubRadius;

        // Segment sampling step, in world units, inside the bounding sphere.
        // The sampled span is at most 2 * kOuterRadius, so this caps the work
        // at ~75 point tests per smoke regardless of how long the ray is.
        constexpr float kSampleStep = 4.0f;

        template <typename T>
        [[nodiscard]] bool readAt(std::uintptr_t base, std::size_t offset, T& out) noexcept
        {
            const std::uintptr_t address = base + offset;
            if (address <= 0x10000 || address >= 0x7FFFFFFFFFFF)
                return false;
            if (IsBadReadPtr(reinterpret_cast<const void*>(address), sizeof(T)))
                return false;
            std::memcpy(&out, reinterpret_cast<const void*>(address), sizeof(T));
            return true;
        }

        // Centre of a live smoke volume, or nothing when the projectile has not
        // detonated yet or its position does not read as a finite point.
        [[nodiscard]] bool volumeCentre(CEntityInstance* entity, entities::Vec3& out) noexcept
        {
            const auto base = reinterpret_cast<std::uintptr_t>(entity);
            if (!base)
                return false;

            // A thrown-but-unexploded smoke occludes nothing. The game sets the
            // tick it began at detonation, and flags the effect separately once
            // it has actually spawned; require both so a projectile still in
            // flight never blocks a shot.
            std::int32_t tickBegin{};
            bool didEffect{};
            if (!readAt(base, g_off.tickBegin, tickBegin) || tickBegin <= 0)
                return false;
            if (!readAt(base, g_off.didEffect, didEffect) || !didEffect)
                return false;

            float position[3]{};
            if (!readAt(base, g_off.detonationPos, position))
                return false;
            if (!std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2]))
                return false;

            out = { position[0], position[1], position[2] + kOriginHeight };
            return true;
        }

        [[nodiscard]] bool insideVolume(const entities::Vec3& point, const entities::Vec3& centre) noexcept
        {
            const float dx = point.x - centre.x;
            const float dy = point.y - centre.y;
            const float dz = point.z - centre.z;

            // Distance from the torus axis, then distance from the ring itself.
            const float axial = std::sqrt(dx * dx + dy * dy) - kRingRadius;
            return axial * axial + dz * dz <= kRingSubRadius * kRingSubRadius;
        }

        // Walks the segment where it overlaps the volume's bounding sphere.
        [[nodiscard]] bool segmentHitsVolume(
            const entities::Vec3& start, const entities::Vec3& end, const entities::Vec3& centre) noexcept
        {
            const float dx = end.x - start.x;
            const float dy = end.y - start.y;
            const float dz = end.z - start.z;

            const float a = dx * dx + dy * dy + dz * dz;
            if (a <= 0.0f)
                return insideVolume(start, centre);

            const float ox = start.x - centre.x;
            const float oy = start.y - centre.y;
            const float oz = start.z - centre.z;

            const float b = 2.0f * (dx * ox + dy * oy + dz * oz);
            const float c = ox * ox + oy * oy + oz * oz - kOuterRadius * kOuterRadius;

            const float discriminant = b * b - 4.0f * a * c;
            if (discriminant < 0.0f)
                return false;

            const float root = std::sqrt(discriminant);
            float enter = (-b - root) / (2.0f * a);
            float exit = (-b + root) / (2.0f * a);
            if (enter < 0.0f)
                enter = 0.0f;
            if (exit > 1.0f)
                exit = 1.0f;
            if (exit < enter)
                return false;

            const float length = std::sqrt(a);
            const float span = (exit - enter) * length;
            // At least the two endpoints of the overlapping span, so a segment
            // that only clips the sphere is still tested rather than skipped.
            const int steps = span > 0.0f ? static_cast<int>(span / kSampleStep) + 1 : 1;

            for (int step = 0; step <= steps; ++step)
            {
                const float t = enter + (exit - enter) * (static_cast<float>(step) / static_cast<float>(steps));
                const entities::Vec3 point{ start.x + dx * t, start.y + dy * t, start.z + dz * t };
                if (insideVolume(point, centre))
                    return true;
            }

            return false;
        }

        // Collects the live volumes. Smokes are few (one per thrown grenade,
        // and they expire), so this stays a handful of entries per call.
        [[nodiscard]] std::vector<entities::Vec3> liveVolumes()
        {
            std::vector<entities::Vec3> centres;
            if (!g_off.resolved)
                return centres;

            for (auto* const entity : g_entitySystem->get("C_SmokeGrenadeProjectile"))
            {
                entities::Vec3 centre{};
                if (volumeCentre(entity, centre))
                    centres.push_back(centre);
            }
            return centres;
        }
    }

    bool available() noexcept { return g_off.resolved; }

    bool init()
    {
        g_off.tickBegin     = SchemaFinder::Get(hash_32_fnv1a_const("C_SmokeGrenadeProjectile->m_nSmokeEffectTickBegin"));
        g_off.didEffect     = SchemaFinder::Get(hash_32_fnv1a_const("C_SmokeGrenadeProjectile->m_bDidSmokeEffect"));
        g_off.detonationPos = SchemaFinder::Get(hash_32_fnv1a_const("C_SmokeGrenadeProjectile->m_vSmokeDetonationPos"));

        g_off.resolved = g_off.tickBegin != 0 && g_off.didEffect != 0 && g_off.detonationPos != 0;

        std::cout << "  smokes: tickBegin=0x" << std::hex << g_off.tickBegin
                  << " didEffect=0x" << g_off.didEffect
                  << " detonationPos=0x" << g_off.detonationPos << std::dec << std::endl;

        if (!g_off.resolved)
            std::cout << "  smokes: [!] schema unresolved - smoke checks will pass through" << std::endl;

        return g_off.resolved;
    }

    // Both queries allocate (the entity system hands back a vector by value),
    // so they catch rather than let an allocation failure terminate the game
    // from inside a noexcept command hook. Failing closed here would refuse
    // every shot, so both fail open: "no smoke known".
    std::size_t activeCount() noexcept
    {
        try
        {
            return liveVolumes().size();
        }
        catch (...)
        {
            return 0;
        }
    }

    bool lineGoesThroughSmoke(const entities::Vec3& start, const entities::Vec3& end) noexcept
    {
        try
        {
            for (const auto& centre : liveVolumes())
            {
                if (segmentHitsVolume(start, end, centre))
                    return true;
            }
        }
        catch (...)
        {
        }
        return false;
    }
}
