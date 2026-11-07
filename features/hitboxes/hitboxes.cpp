#include "hitboxes.h"

#include "../../utils/schema/schema.h"
#include "../../utils/fnv1a/fnv1a.h"

#include <Windows.h>

#include <cstring>
#include <iostream>

namespace tw::hitboxes
{
    namespace
    {
        // ------------------------------------------------------------------
        // CHitBox is declared in animationsystem.dll's schema scope, so every
        // offset below is resolved from the game at runtime. The literals are
        // only a fallback and were read out of the static schema descriptor
        // tables in that DLL (CHitBox: size 0x70, 13 fields) rather than copied
        // from another project.
        //
        // Reaching a hitbox does NOT go through the model. Walking
        // PermModelData_t for a CHitBox array was tried and does not work on
        // this build: a bounded search visiting 2213 objects three levels deep
        // from the model resource found no such array, so the geometry is not
        // hung off it. The trace result carries the CHitBox it struck instead,
        // which is cheaper and is the game's own answer to "which body part".
        // ------------------------------------------------------------------
        struct Offsets
        {
            std::uint32_t name{};
            std::uint32_t boneName{};
            std::uint32_t mins{};
            std::uint32_t maxs{};
            std::uint32_t radius{};
            std::uint32_t group{};
            std::uint32_t shapeType{};
            std::uint32_t translationOnly{};
            std::uint32_t index{};
            bool          resolved{};
        };

        Offsets g_off{};

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

        [[nodiscard]] const char* readName(std::uintptr_t base, std::size_t offset) noexcept
        {
            std::uintptr_t pointer{};
            if (!readAt(base, offset, pointer))
                return nullptr;
            if (pointer <= 0x10000 || pointer >= 0x7FFFFFFFFFFF)
                return nullptr;
            if (IsBadReadPtr(reinterpret_cast<const void*>(pointer), 2))
                return nullptr;

            const char* text = reinterpret_cast<const char*>(pointer);
            for (std::size_t i = 0; i < 64; ++i)
            {
                if (IsBadReadPtr(text + i, 1))
                    return nullptr;
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if (c == 0)
                    return i > 0 ? text : nullptr;
                if (c < 0x20 || c > 0x7E)
                    return nullptr;
            }
            return nullptr;
        }
    }

    bool available() noexcept { return g_off.resolved; }

    bool init()
    {
        const auto get = [](const char* field, std::uint32_t fallback) {
            return SchemaFinder::GetOr(hash_32_fnv1a_const(field), fallback);
        };

        g_off.name            = get("CHitBox->m_name", 0x00);
        g_off.boneName        = get("CHitBox->m_sBoneName", 0x10);
        g_off.mins            = get("CHitBox->m_vMinBounds", 0x18);
        g_off.maxs            = get("CHitBox->m_vMaxBounds", 0x24);
        g_off.radius          = get("CHitBox->m_flShapeRadius", 0x30);
        g_off.group           = get("CHitBox->m_nGroupId", 0x38);
        g_off.shapeType       = get("CHitBox->m_nShapeType", 0x3C);
        g_off.translationOnly = get("CHitBox->m_bTranslationOnly", 0x3D);
        g_off.index           = get("CHitBox->m_nHitBoxIndex", 0x48);

        const bool fromSchema = SchemaFinder::Has(hash_32_fnv1a_const("CHitBox->m_nGroupId"));
        g_off.resolved = true;

        std::cout << "  hitboxes: CHitBox group=0x" << std::hex << g_off.group
                  << " radius=0x" << g_off.radius
                  << " mins=0x" << g_off.mins << std::dec
                  << (fromSchema ? " [schema]" : " [FALLBACK]") << std::endl;

        return g_off.resolved;
    }

    int groupOf(std::uintptr_t hitboxData) noexcept
    {
        if (!g_off.resolved || !hitboxData)
            return kInvalid;

        int group{};
        if (!readAt(hitboxData, g_off.group, group))
            return kInvalid;

        // The game only defines groups 0..8; anything else means the pointer is
        // not a CHitBox and the value must not be trusted.
        return (group >= kGeneric && group <= kNeck) ? group : kInvalid;
    }

    bool describe(std::uintptr_t hitboxData, Hitbox& out) noexcept
    {
        const int group = groupOf(hitboxData);
        if (group == kInvalid)
            return false;

        std::uint8_t translationOnly{};
        if (!readAt(hitboxData, g_off.mins, out.mins) ||
            !readAt(hitboxData, g_off.maxs, out.maxs) ||
            !readAt(hitboxData, g_off.radius, out.radius) ||
            !readAt(hitboxData, g_off.shapeType, out.shapeType) ||
            !readAt(hitboxData, g_off.translationOnly, translationOnly) ||
            !readAt(hitboxData, g_off.index, out.index))
            return false;

        if (!(out.radius >= 0.0f && out.radius <= 100.0f))
            return false;

        out.group           = group;
        out.translationOnly = translationOnly != 0;
        out.name            = readName(hitboxData, g_off.name);
        out.boneName        = readName(hitboxData, g_off.boneName);
        return true;
    }

    const char* groupName(int group) noexcept
    {
        switch (group)
        {
        case kHead:     return "head";
        case kChest:    return "chest";
        case kStomach:  return "stomach";
        case kLeftArm:  return "left arm";
        case kRightArm: return "right arm";
        case kLeftLeg:  return "left leg";
        case kRightLeg: return "right leg";
        case kNeck:     return "neck";
        case kGeneric:  return "body";
        default:        return "?";
        }
    }
}
