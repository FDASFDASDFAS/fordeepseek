#pragma once
#include <cstdint>
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../C_CSWeaponBase/C_CSWeaponBase.h"
#include <cstdint>

class c_aggregate_object_data
{
public:
	char pad_0000[4]; //0x0000
	int count; //0x0004
	char pad_0008[40]; //0x0008
	int index; //0x0030
}; //Size: 0x0034

class c_aggregate_object_array
{
public:
	void* object; //0x0000
	c_aggregate_object_data* data; //0x0008
};

class CMaterial2
{
public:
	virtual const char* GetName() = 0;
	virtual const char* GetShareName() = 0;
};

struct MaterialKeyVar_t
{
	std::uint64_t uKey;
	const char* szName;

	MaterialKeyVar_t(std::uint64_t uKey, const char* szName) :
		uKey(uKey), szName(szName) { }

	MaterialKeyVar_t(const char* szName, bool bShouldFindKey = false) :
		szName(szName)
	{
		uKey = bShouldFindKey ? FindKey(szName) : 0x0;
	}

	std::uint64_t FindKey(const char* szName)
	{
		using fn = std::uint64_t(__fastcall*)(const char*, unsigned int, int);
		static auto find = reinterpret_cast<fn>(M::patternScan("particles", ("48 89 5C 24 ? 57 48 81 EC ? ? ? ? 33 C0")));
		return find(szName, 0x12, 0x31415926);
	}
};

class CObjectInfo
{
	MEM_PAD(0xB0);
	int nId;
};

class CSceneAnimatableObject {
public:
	CBaseHandle Owner() const {
		if (!this)
			return CBaseHandle();

		return *(CBaseHandle*)((std::uintptr_t)this + 0x0C0);
	}
};  

class Color_tr
{
public:
	uint8_t r, g, b, a;

public:
	Color_tr()
		: r(0), g(0), b(0), a(255) {
	}
	Color_tr(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
		: r(red), g(green), b(blue), a(alpha) {
	}
	Color_tr(const float* col)
		: r((uint8_t)(col[0] * 255.f)), g((uint8_t)(col[1] * 255.f)), b((uint8_t)(col[2] * 255.f)), a((uint8_t)(col[3] * 255.f)) {
	}
	Color_tr(const Color_tr& col)
		: r(col.r), g(col.g), b(col.b), a(col.a) {
	}
};

class CMeshData
{
public:
	uint8_t pad_00[0x18];      // 0x00
	CSceneAnimatableObject* pSceneAnimatableObject; // 0x18
	CMaterial2* pMaterial; // 0x20
	CMaterial2* pMaterial2; // 0x28
	uint8_t pad_30[0x20];      // 0x30
	Color_tr color; // 0x50
	uint8_t pad_54[0x1C];      // 0x54
};

