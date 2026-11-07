#pragma once
#include "includeHooks.h"
#include "../../cs2/entity/C_AggregateSceneObject/C_AggregateSceneObject.h"
#include "../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../cs2/datatypes/cutlbuffer/cutlbuffer.h"
#include "../../cs2/datatypes/keyvalues/keyvalues.h"
#include "../../cs2/entity/C_Material/C_Material.h"
#include "../interfaces/CCSGOInput/CCSGOInput.h"
#include "../commands/command_pipeline.h"
#include "../runtime/runtime.h"
#include "../../cs2/entity/C_Material/C_Material.h"

// Forward declaration
class CMeshData;
class CEntityIdentity;

namespace H {
	void __fastcall hkFrameStageNotify(void* a1, int stage);
	void* __fastcall hkLevelInit(__int64 a1, __int64 a2);
	void* __fastcall hkOnLevelShutdown(void* a1, const char* map_name);
	void __fastcall hkChamsObject(void* pAnimatableSceneObjectDesc, void* pDx11, CMeshData* arrMeshDraw, int nDataCount, void* pSceneView, void* pSceneLayer, void* pUnk, void* pUnk2);
	// GeneratePrimitives hook (scenesystem.dll) — the approach verified in velocity-main and cs2-universal-offsets
	// (build 14172). Modifies primitive buffer entries (0x68-byte primitives) rather than CMeshData fields.
	void __fastcall hkGeneratePrimitives(void* a1, void* Object, void* a3, void* RenderBuffer);
	// Not currently hooked: the "FlashOverlay" pattern in cs2-universal-offsets targets a different,
	// unidentified function (confirmed wrong -- see the schema-write anti-flash in hkFrameStageNotify
	// instead). Left declared with its originally-assumed 5-param shape for whenever a verified pattern
	// for the real draw_flash_effect-equivalent function is found.
	void __fastcall hkRenderFlashbangOverlay(void* a1, void* a2, void* a3, void* a4, void* a5);
	void __fastcall hkCreateMove(CCSGOInput* rcx, int slot, bool active);
	void* hkOnAddEntity(void* a1, CEntityInstance* entity_instance, int handle);
	void* hkOnRemoveEntity(void* a1, CEntityInstance* entity_instance, int handle);
	inline float g_flActiveFov;
	float hkGetRenderFov(void* rcx);
	void __fastcall hkMoveCrcBoundary(CCSGOInput* rcx, CUserCmd* command);

	// World
	void* __fastcall hkUpdateAggregateSceneObject(void* a1, void* a2, c_aggregate_object_array* a3);
	void __fastcall hkUpdateLightObject(__int64 a1, __int64 a2, __int64 a3);
	void RestoreWorldState();
	void ClearWorldState();
	void RestoreLevelState();
	[[nodiscard]] tw::commands::CommandProbeSnapshot GetCommandProbeSnapshot() noexcept;

	inline CInlineHookObj<decltype(&hkChamsObject)> DrawArray = { };
	inline CInlineHookObj<decltype(&hkGeneratePrimitives)> GeneratePrimitives = { };
	inline CInlineHookObj<decltype(&hkFrameStageNotify)> FrameStageNotify = { };
	inline CInlineHookObj<decltype(&hkGetRenderFov)> GetRenderFov = { };
	inline CInlineHookObj<decltype(&hkLevelInit)> LevelInit = { };
	inline CInlineHookObj<decltype(&hkOnLevelShutdown)> LevelShutdown = { };
	inline CInlineHookObj<decltype(&hkRenderFlashbangOverlay)> RenderFlashBangOverlay = { };
	inline CInlineHookObj<decltype(&hkCreateMove)> CreateMove = { };
	inline CInlineHookObj<decltype(&hkMoveCrcBoundary)> MoveCrcBoundary = { };
	inline CInlineHookObj<decltype(&hkUpdateAggregateSceneObject)> UpdateAggregateSceneObject = { };
	inline CInlineHookObj<decltype(&hkUpdateLightObject)> UpdateLightObject = { };
	inline CInlineHookObj<decltype(&hkOnAddEntity)> OnAddEntity = { };
	inline CInlineHookObj<decltype(&hkOnRemoveEntity)> OnRemoveEntity = { };

	// inline hooks
	inline int  oGetWeaponData;
	inline void* (__fastcall* ogGetBaseEntity)(void*, int);

	class Hooks {
	public:
		bool init();
		bool disable();
		bool shutdown();
	};
}
