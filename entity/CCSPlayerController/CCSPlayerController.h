#pragma once
#include <cstdint>
#pragma once
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../C_CSWeaponBase/C_CSWeaponBase.h"
#include <cstdint>
class CCSPlayerController {
public:
	CCSPlayerController(uintptr_t address);
	const char* getName() const;
	uintptr_t getAddress() const;

	schema(bool, IsLocalPlayer, "CBasePlayerController->m_bIsLocalPlayerController");
	schema(CBaseHandle, m_hPawn, "CBasePlayerController->m_hPawn");
	schema(CBaseHandle, m_hObserverPawn, "CCSPlayerController->m_hObserverPawn");
	schema(const char*, m_sSanitizedPlayerName, "CCSPlayerController->m_sSanitizedPlayerName");
	schema(bool, m_bPawnIsAlive, "CCSPlayerController->m_bPawnIsAlive");
	schema(int, m_nTickBase, "CBasePlayerController->m_nTickBase");

private:
	uintptr_t address;
};
