#include "C_CSWeaponBase.h"
#include "..\..\..\templeware\hooks\hooks.h"
CCSWeaponBaseVData* C_CSWeaponBase::Data()
{
	// return pointer to weapon data
	return *reinterpret_cast<CCSWeaponBaseVData**>((uintptr_t)this + H::oGetWeaponData);
}

float C_CSWeaponBase::get_spread()
{
    // Pattern: "48 83 EC ? 48 63 91"
    using fn_get_spread_t = float(__fastcall*)(void*);
    static fn_get_spread_t fn = reinterpret_cast<fn_get_spread_t>(M::FindPattern("client.dll", "48 83 EC ? 48 63 91"));

    return fn(this);
}


void C_CSWeaponBase::update_accuracy()
{
    using fn_update_accuracy_penality_t = void(__fastcall*)(void*);
    static fn_update_accuracy_penality_t fn = reinterpret_cast<fn_update_accuracy_penality_t>(M::patternScan("client", "40 53 57 48 83 EC ? 48 8B F9 E8"));

    fn(this);
}

float C_CSWeaponBase::get_inaccuracy()
{
    float x = 0.0f, y = 0.0f;

    using fn_get_inaccuracy_t = float(__fastcall*)(void*, float*, float*);
    static fn_get_inaccuracy_t fn = reinterpret_cast<fn_get_inaccuracy_t>(M::FindPattern("client.dll", "48 89 5C 24 ? 55 56 57 48 81 EC ? ? ? ? 44 0F 29 84 24"));

    return fn(this, &x, &y);
}