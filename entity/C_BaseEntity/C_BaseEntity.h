#pragma once
#include <cstdint>
#include "../C_EntityInstance/C_EntityInstance.h"
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../../source/templeware/utils/schema/schema.h"
#include "../../../../source/templeware/utils/memory/vfunc/vfunc.h"
#include "../handle.h"
#include "../CGameSceneNode/CGameSceneNode.h"

class CCSWeaponBaseVData;

class C_BaseEntity : public CEntityInstance
{
public:
	schema(int, m_iMaxHealth, "C_BaseEntity->m_iMaxHealth");
	schema(CBaseHandle, m_hPawn, "CBasePlayerController->m_hPawn");
	schema(std::int32_t, m_iHealth, "C_BaseEntity->m_iHealth");
	schema(std::uint8_t, m_iTeamNum, "C_BaseEntity->m_iTeamNum");
	schema(std::uint32_t, m_fFlags, "C_BaseEntity->m_fFlags");
	// Declared as a single byte, not int32: MoveType_t is schema-narrow in this build. Reading it as
	// int32 picks up 3 bytes of adjacent memory as noise -- live telemetry observed 514 (0x0202) while
	// walking, and templeware/features/rage/antiaim.cpp had independently hardcoded 1794 (0x0702) and
	// 2313 (0x0909) for walk/ladder, all three sharing the true low-byte ordinal (2, 2, 9) with
	// unrelated garbage upper bytes.
	schema(std::uint8_t, m_MoveType, "C_BaseEntity->m_MoveType");
	schema(CBaseHandle, m_hController, "C_BasePlayerPawn->m_hController");
	schema(Vector_t, m_vecAbsVelocity, "C_BaseEntity->m_vecAbsVelocity");
	schema(Vector_t, m_vecVelocity, "C_BaseEntity->m_vecVelocity");
	[[nodiscard]] inline std::add_lvalue_reference_t<CCSWeaponBaseVData*> m_pWeaponData() {
		static const uint32_t baseOffset = SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_nSubclassID")); static const uint32_t totalOffset = baseOffset + (0x8); return *reinterpret_cast<std::add_pointer_t<CCSWeaponBaseVData*>>(reinterpret_cast<uint8_t*>(this) + totalOffset);
	};
	schema(int, m_nSubclassID, "C_BaseEntity->m_nSubclassID");
	schema(CGameSceneNode*, m_pGameSceneNode, "C_BaseEntity->m_pGameSceneNode");
	// Held/thrown-by ownership for world entities (dropped weapons, grenades): valid when the entity is
	// carried or otherwise attached to an owner, invalid when it's genuinely lying in the world.
	schema(CBaseHandle, m_hOwnerEntity, "C_BaseEntity->m_hOwnerEntity");

	// Generic world position for any entity (grenades, dropped weapons, the bomb), not just player
	// pawns -- distinct from C_CSPlayerPawn's own m_vOldOrigin, which is interpolated for rendering.
	[[nodiscard]] Vector_t origin()
	{
		auto* scene = m_pGameSceneNode();
		return scene ? scene->GetAbsOrigin() : Vector_t{};
	}

	bool IsBasePlayer()
	{
		SchemaClassInfoData_t* pClassInfo;
		dump_class_info(&pClassInfo);
		if (pClassInfo == nullptr)
			return false;

		return hash_32_fnv1a_const(pClassInfo->szName) == hash_32_fnv1a_const("C_CSPlayerPawn");
	}

	bool IsViewmodelAttachment()
	{
		SchemaClassInfoData_t* pClassInfo;
		dump_class_info(&pClassInfo);
		if (pClassInfo == nullptr)
			return false;

		return hash_32_fnv1a_const(pClassInfo->szName) == hash_32_fnv1a_const("C_CS2HudModelArms");
	}

	bool IsViewmodel()
	{
		SchemaClassInfoData_t* pClassInfo;
		dump_class_info(&pClassInfo);
		if (pClassInfo == nullptr)
			return false;

		return hash_32_fnv1a_const(pClassInfo->szName) == hash_32_fnv1a_const("C_CS2HudModelWeapon");
	}

	bool IsPlayerController()
	{
		SchemaClassInfoData_t* _class = nullptr;
		dump_class_info(&_class);
		if (!_class)
			return false;

		const uint32_t hash = hash_32_fnv1a_const(_class->szName);

		return (hash == hash_32_fnv1a_const("CCSPlayerController"));
	}
};
