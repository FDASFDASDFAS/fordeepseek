#include "C_CSPlayerPawn.h"

#include "../../../templeware/offsets/offsets.h"
#include "../../../templeware/interfaces/interfaces.h"

C_CSPlayerPawn::C_CSPlayerPawn(uintptr_t address) : address(address) {}

Vector_t C_CSPlayerPawn::getPosition() const {
	return *(Vector_t*)(address + SchemaFinder::Get(hash_32_fnv1a_const("C_BasePlayerPawn->m_vOldOrigin")));
}

Vector_t C_CSPlayerPawn::getEyePosition() const {
	return Vector_t();
}

C_CSWeaponBase* C_CSPlayerPawn::GetActiveWeapon() {
	if (!this)
		return nullptr;

	C_CSWeaponBase* pWeapon = nullptr;
	if (auto pWeaponServices = this->m_pWeaponServices()) {
		CBaseHandle hWeapon = pWeaponServices->m_hActiveWeapon();
		if (hWeapon.valid() && I::GameEntity && I::GameEntity->Instance)
			pWeapon = I::GameEntity->Instance->Get<C_CSWeaponBase>(hWeapon);
	}

	return pWeapon;
}

CCSPlayer_WeaponServices* C_CSPlayerPawn::GetWeaponServices() {
	return this ? this->m_pWeaponServices() : nullptr;
}

uintptr_t C_CSPlayerPawn::getAddress() const {
	return address;
}

int C_CSPlayerPawn::getHealth() const {
	return *reinterpret_cast<int*>((uintptr_t)this + SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iHealth")));
}

uint8_t C_CSPlayerPawn::getTeam() const {
	return *reinterpret_cast<uint8_t*>((uintptr_t)this + SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iTeamNum")));
}

Vector_t C_CSPlayerPawn::getViewOffset() const {
	return *reinterpret_cast<Vector_t*>((uintptr_t)this + SchemaFinder::Get(hash_32_fnv1a_const("C_BaseModelEntity->m_vecViewOffset")));
}
