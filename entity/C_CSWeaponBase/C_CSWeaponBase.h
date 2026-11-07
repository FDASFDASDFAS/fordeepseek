#pragma once
#include <cstdint>
#include "..\C_EntityInstance\C_EntityInstance.h"
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "..\..\..\..\source\templeware\utils\schema\schema.h"
#include "..\..\..\..\source\templeware\utils\memory\vfunc\vfunc.h"
#include "..\handle.h"
#include "../../../cs2/entity/C_BaseEntity/C_BaseEntity.h"

class C_BaseEntity;

class CCSPlayer_WeaponServices
{
public:
	schema(bool, m_bAllowSwitchToNoWeapon, "CPlayer_WeaponServices->m_bAllowSwitchToNoWeapon");
	schema(CBaseHandle, m_hMyWeapons, "CPlayer_WeaponServices->m_hMyWeapons");
	schema(CBaseHandle, m_hActiveWeapon, "CPlayer_WeaponServices->m_hActiveWeapon");
	schema(CBaseHandle, m_hLastWeapon, "CPlayer_WeaponServices->m_hLastWeapon");
	schema(int, m_iAmmo, "CPlayer_WeaponServices->m_iAmmo");
	schema(float, m_flNextAttack, "CCSPlayer_WeaponServices->m_flNextAttack");
	schema(bool, m_bIsLookingAtWeapon, "CCSPlayer_WeaponServices->m_bIsLookingAtWeapon");
	schema(bool, m_bIsHoldingLookAtWeapon, "CCSPlayer_WeaponServices->m_bIsHoldingLookAtWeapon");
};

class CCSWeaponBaseVData
{
public:
	SCHEMA_ADD_OFFSET(const char*, m_szName, 0x720);
};

class C_CSWeaponBase : public C_BaseEntity
{
public:
	schema(int, m_iBurstShotsRemaining, "CBasePlayerWeapon->m_iBurstShotsRemaining");
	schema(int, m_iBurstShotsRemainingOld, "CBasePlayerWeapon->m_iBurstShotsRemainingOld");
	schema(bool, m_bBurstMode, "CBasePlayerWeapon->m_bBurstMode");
	schema(bool, m_bSilencerOn, "CBasePlayerWeapon->m_bSilencerOn");
	schema(int, m_zoomLevel, "CWeaponCSBase->m_zoomLevel");
	schema(int32_t, clip1, "C_BasePlayerWeapon->m_iClip1");
	schema(int, m_weaponMode, "C_CSWeaponBase->m_weaponMode");
	schema(float, m_flPostponeFireReadyTime, "CWeaponCSBase->m_flPostponeFireReadyTime");
	schema(bool, m_bInReload, "C_CSWeaponBase->m_bInReload");
	schema(bool, m_bInspectPending, "C_CSWeaponBase->m_bInspectPending");
	schema(int, m_nPostponeFireReadyTicks, "C_CSWeaponBase->m_nPostponeFireReadyTicks");
	schema(int, m_iWeaponGameplayAnimState, "C_CSWeaponBase->m_iWeaponGameplayAnimState");
	schema(float, m_flRecoilIndex, "C_CSWeaponBase->m_flRecoilIndex");
	schema(float, m_fAccuracyPenalty, "C_CSWeaponBase->m_fAccuracyPenalty")
	schema(int, m_nNextPrimaryAttackTick, "C_BasePlayerWeapon->m_nNextPrimaryAttackTick");

// @todo: add few schemas here
	CCSWeaponBaseVData* Data();

	void update_accuracy();
	float get_inaccuracy();

	float get_max_speed() {
		return M::CallVFunc<float, 340U>(this);
	}

	//364
	float get_spread();
};
