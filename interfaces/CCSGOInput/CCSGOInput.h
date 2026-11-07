#pragma once
#include <Windows.h>
#include <cstdio>
#include "../../utils/math/vector/vector.h"
#include "../../hooks/includeHooks.h"
#include "../../utils/memory/patternscan/patternscan.h"
#include "../CUserCmd/CUserCmd.h"

#define MULTIPLAYER_BACKUP 150

class CTinyMoveStepData
{
public:
	float flWhen; //0x0000
	MEM_PAD(0x4); //0x0004
	std::uint64_t nButton; //0x0008
	bool bPressed; //0x0010
	MEM_PAD(0x7); //0x0011
}; //Size: 0x0018

class CMoveStepButtons
{
public:
	std::uint64_t nKeyboardPressed; //0x0000
	std::uint64_t nMouseWheelheelPressed; //0x0008
	std::uint64_t nUnPressed; //0x0010
	std::uint64_t nKeyboardCopy; //0x0018
}; //Size: 0x0020

// @credits: www.unknowncheats.me/forum/members/2943409.html
class CExtendedMoveData : public CMoveStepButtons
{
public:
	float flForwardMove; //0x0020
	float flSideMove; //0x0024
	float flUpMove; //0x0028
	std::int32_t nMouseDeltaX; //0x002C
	std::int32_t nMouseDeltaY; //0x0030
	std::int32_t nAdditionalStepMovesCount; //0x0034
	CTinyMoveStepData tinyMoveStepData[12]; //0x0038
	Vector_t vecViewAngle; //0x0158
	std::int32_t nTargetHandle; //0x0164
}; //Size:0x0168

class CCSGOInput
{
public:
	MEM_PAD(0x228);
	void* arrCommands[MULTIPLAYER_BACKUP];
	MEM_PAD(0x99)
		bool bInThirdPerson;
	MEM_PAD(0x6);
	QAngle_t angThirdPersonAngles;
	MEM_PAD(0xE);
	std::int32_t nSequenceNumber;
	double dbSomeTimer;
	CExtendedMoveData currentMoveData;
	std::int32_t nWeaponSwitchTick;
	MEM_PAD(0x1C4);
	CExtendedMoveData* pExtendedMoveData;
	MEM_PAD(0x48);
	int32_t nAttackStartHistoryIndex1;
	int32_t nAttackStartHistoryIndex2;
	int32_t nAttackStartHistoryIndex3;

	CUserCmd* get_user_cmd(void* local_controller)
	{
		if (!local_controller)
			return nullptr;

		typedef uintptr_t(__fastcall* get_local_controller_by_internal_id_fn)(int);
		static get_local_controller_by_internal_id_fn get_local_controller_by_internal_id = (get_local_controller_by_internal_id_fn)M::scan("client.dll", "48 83 EC ?? 83 F9 ?? 75 ?? 48 8B 0D ?? ?? ?? ?? 48 8D 54 24 ?? 48 8B 01 FF 90 ?? ?? ?? ?? 8B 08 48 63 C1 48 8D 0D ?? ?? ?? ?? 48 8B 04 C1 48 83 C4 ?? C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 48 83 EC ?? 83 F9");
		typedef uintptr_t(__fastcall* setup_cmd_fn)(uintptr_t);
		static setup_cmd_fn setup_cmd = (setup_cmd_fn)M::scan("client.dll", "48 83 EC 28 E8 ?? ?? ?? ?? 8B 80");
		typedef uintptr_t(__fastcall* get_controller_cmd_fn)(uintptr_t, uintptr_t);
		static get_controller_cmd_fn get_controller_cmd = (get_controller_cmd_fn)M::scan("client.dll", "40 53 48 83 EC 20 8B DA E8 ?? ?? ?? ?? 4C");
		if (!get_local_controller_by_internal_id || !setup_cmd || !get_controller_cmd)
			return nullptr;

		auto controller = get_local_controller_by_internal_id(0);
		if (!controller)
			return nullptr;
		return (CUserCmd*)get_controller_cmd(controller, setup_cmd(controller));
	}

	// 85 D2 75 ? 48 63 81 ...
	void SetViewAngle(Vector_t& angView)
	{
		using fnSetViewAngle = std::int64_t(_fastcall*)(CCSGOInput*, int, Vector_t&);

		static fnSetViewAngle oSetViewAngle = reinterpret_cast<fnSetViewAngle>(M::FindPattern("client", "85 D2 75 ? 48 63 81"));

		if (!oSetViewAngle)
		{
			return;
		}

		oSetViewAngle(this, 0, angView);
	}


	// 4C 8B C1 85 D2 74 08 48 8D 05 ? ? ? ? C3
	Vector_t GetViewAngles()
	{
		using fnGetViewAngles = std::int64_t(_fastcall*)(CCSGOInput*, std::int32_t);
		static auto oGetViewAngles = reinterpret_cast<fnGetViewAngles>(M::FindPattern("client", ("4C 8B C1 85 D2 74 08 48 8D 05 ? ? ? ? C3")));

		return *reinterpret_cast<Vector_t*>(oGetViewAngles(this, 0));
	}
};
