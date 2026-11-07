#pragma once
#include <cstdint>
#include "../../../cs2/entity/C_BaseEntity/C_BaseEntity.h"
#include "../../../cs2/entity/C_Material/C_Material.h"
#include "../../../cs2/datatypes/utlstronghandle/utlstronghandle.h"

// forward declarations
class CMeshData;

enum ChamsType {
	FLAT,
	ILLUMINATE,
	GLOW,
	MAXCOUNT
};

enum ChamsEntity : std::int32_t {
	INVALID = 0,
	ENEMY,
	TEAM,
	VIEWMODEL,
	LOCAL,
	HANDS
};

// Primitive entry in GeneratePrimitives output buffer — 0x70 bytes each.
// CS2 update increased trailing padding _pad3 from 0x14 to 0x1C (total size 0x70).
// Material at +0x20, Material2 at +0x28, TintColor at +0x50.
struct ChamsPrimitive
{
	char pad_0x00[0x20];
	void* Material;               // +0x20
	void* Material2;              // +0x28
	char pad_0x30[0x20];
	std::uint32_t TintColor;      // +0x50
	char pad_0x54[0x1C];          // trailing to 0x70
};
static_assert(sizeof(ChamsPrimitive) == 0x70, "ChamsPrimitive must be 0x70 bytes");

// Field offsets within a primitive. These are stable across the builds seen so
// far; the distance *between* primitives is not, so it is measured at runtime
// rather than taken from sizeof(ChamsPrimitive) -- see strideFor in chams.cpp.
inline constexpr std::size_t kPrimitiveMaterialOffset = 0x20;
inline constexpr std::size_t kPrimitiveTintOffset     = 0x50;

// CMeshPrimitiveOutputBuffer — the buffer passed to GeneratePrimitives as RenderBuffer.
struct CMeshPrimitiveOutputBuffer
{
	ChamsPrimitive* m_pOut;
	int m_nMaximumOutputPrimitives;
	int m_nStartPrimitives;
};

// Offsets into CSceneAnimatableObject (defined in C_Material.h — NOT redefined here)
inline constexpr std::uint32_t kSceneObjectFlagsOffset = 0x78;
inline constexpr std::uint32_t kSceneObjectOwnerOffset = 0xC0;

namespace chams
{
	class Materials {
	public:
		bool init();
	};

	static ChamsEntity GetTargetType(C_BaseEntity* entity) noexcept;
	C_StrongHandle<CMaterial2> create(const char* name, const char szVmatBuffer[]);
	void __fastcall hook(void* pAnimatableSceneObjectDesc, void* pDx11, CMeshData* arrMeshDraw, int nDataCount, void* pSceneView, void* pSceneLayer, void* pUnk, void* pUnk2);
	[[nodiscard]] CMaterial2* GetMaterialPtr(int type, bool invisible) noexcept;
	void __fastcall hkGeneratePrimitives(void* a1, void* Object, void* a3, CMeshPrimitiveOutputBuffer* RenderBuffer);

	// Pre-cached chams targets, populated on the game thread (FrameStageNotify),
	// read on the render thread (GeneratePrimitives). Uses 15-bit entity index (& 0x7FFF)
	// which is 100% immune to serial bit differences across handles.
	inline std::uint32_t cachedOwnerIndices[128]{};
	inline ChamsEntity cachedOwnerTypes[128]{};
	inline int cachedOwnerCount{};
	void cacheChamsTargets();
}