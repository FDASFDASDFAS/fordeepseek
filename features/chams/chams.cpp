#include <algorithm>
#include <iostream>
#include "chams.h"
#include "../../hooks/hooks.h"
#include "../../runtime/runtime.h"
#include "../../config/configmanager.h"
#include "../model_preview/model_preview.h"
#include "../../../../external/imgui/imgui.h"
#include "../../../cs2/datatypes/utlstronghandle/utlstronghandle.h"
#include "../../../cs2/entity/C_Material/C_Material.h"
#include "../../interfaces/interfaces.h"
#include "../../interfaces/CGameEntitySystem/CGameEntitySystem.h"
#include "../../../cs2/entity/C_EntitySystem/entity.h"
#include "../../../cs2/entity/CCSPlayerController/CCSPlayerController.h"
#include "../../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../../cs2/datatypes/keyvalues/keyvalues.h"
#include "../../../cs2/datatypes/cutlbuffer/cutlbuffer.h"
#include <atomic>
#include <cstdio>
#include <cstring>

C_StrongHandle<CMaterial2> chams::create(const char* name, const char szVmatBuffer[])
{
    CKeyValues3* pKeyValues3 = nullptr;

    pKeyValues3 = pKeyValues3->create_material_from_resource();

    pKeyValues3->LoadFromBuffer(szVmatBuffer);

    C_StrongHandle<CMaterial2> pCustomMaterial = {};

    static const ULONGLONG sessionId = GetTickCount64();
    static std::atomic_uint materialId{ 0 };
    constexpr std::size_t nameCapacity = 260;
    char* persistentName = static_cast<char*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, nameCapacity));
    if (persistentName == nullptr)
        return pCustomMaterial;

    const char* extension = std::strstr(name, ".vmat");
    const int prefixLength = extension != nullptr ? static_cast<int>(extension - name) : static_cast<int>(std::strlen(name));
    std::snprintf(persistentName, nameCapacity, "%.*s_tw_%llu_%u%s", prefixLength, name,
        static_cast<unsigned long long>(sessionId), materialId.fetch_add(1), extension != nullptr ? extension : "");

    // Source 2 may retain the resource-name pointer, so it must outlive this DLL.
    I::CreateMaterial(nullptr, &pCustomMaterial, persistentName, pKeyValues3, 0, 1);

    return pCustomMaterial;
}

struct resource_material_t
{
    C_StrongHandle<CMaterial2> mat;
    C_StrongHandle<CMaterial2> mat_invs;
};

static resource_material_t resourceMaterials[ChamsType::MAXCOUNT];
bool loaded_materials = false;

bool chams::Materials::init()
{
	// Flat material. Body and render-state flags taken verbatim from
	// a known-good reference implementation, so the material definition itself
	// stops being a variable.
	resourceMaterials[FLAT] = resource_material_t{
		.mat = create("materials/dev/flat_vis.vmat", R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"

    F_RENDER_BACKFACES = 0
    F_DISABLE_Z_BUFFERING = 0
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1

    g_vColorTint = [1.0, 1.0, 1.0, 1.0]

    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})"),
		.mat_invs = create("materials/dev/flat_invis.vmat", R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"

    F_RENDER_BACKFACES = 0
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1

    g_vColorTint = [1.0, 1.0, 1.0, 1.0]

    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})")
	};

	// Glow material (csgo_effects.vfx with Fresnel glow)
	resourceMaterials[GLOW] = resource_material_t{
		.mat = create("materials/dev/glow_vis.vmat", R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.000000, 1.000000, 1.000000, 1.000000]
	g_flFresnelExponent = 1.500000
	g_flFresnelFalloff = 5.000000
	g_flFresnelMax = 0.000000
	g_flFresnelMin = 1.000000
	g_flColorBoost = 10.000000
	g_flOpacityScale = 0.800000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})"),
		.mat_invs = create("materials/dev/glow_invis.vmat", R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.000000, 1.000000, 1.000000, 1.000000]
	g_flFresnelExponent = 1.500000
	g_flFresnelFalloff = 5.000000
	g_flFresnelMax = 0.000000
	g_flFresnelMin = 1.000000
	g_flColorBoost = 10.000000
	g_flOpacityScale = 0.800000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})")
	};

    return true;
}

ChamsEntity chams::GetTargetType(C_BaseEntity* render_ent) noexcept {
    if (!render_ent)
        return ChamsEntity::INVALID;

    C_CSPlayerPawn* local = g_ctx->local_pawn;
    if (!local)
        return ChamsEntity::INVALID;

    if (render_ent->IsViewmodelAttachment())
        return ChamsEntity::HANDS;

    if (render_ent->IsViewmodel())
        return ChamsEntity::VIEWMODEL;

    if (!render_ent->IsBasePlayer() && !render_ent->IsPlayerController())
        return ChamsEntity::INVALID;

    auto player = (C_CSPlayerPawn*)render_ent;
    if (!player)
        return ChamsEntity::INVALID;

    if (player->m_iHealth() <= 0)
        return ChamsEntity::INVALID;

    if (player == local)
        return ChamsEntity::LOCAL;

    if (player->m_iTeamNum() == local->m_iTeamNum())
        return ChamsEntity::TEAM;

    return ChamsEntity::ENEMY;
}

CMaterial2* GetMaterial(int type, bool invisible) { return invisible ? resourceMaterials[type].mat_invs : resourceMaterials[type].mat; }

void __fastcall chams::hook(void* a1, void* a2, CMeshData* pMeshScene, int nMeshCount, void* pSceneView, void* pSceneLayer, void* pUnk, void* pUnk2)
{
    Runtime::CallbackGuard callback;
    auto original = H::DrawArray.GetOriginal();
    if (Runtime::IsShuttingDown())
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
	const auto settings = internal_config::ConfigManager::Snapshot();
	const auto& config = settings->chams;

    if (!I::EngineClient->valid())
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    C_CSPlayerPawn* local_player = g_ctx->local_pawn;
    if (!local_player)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    if (!pMeshScene)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    if (!pMeshScene->pSceneAnimatableObject)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    if (nMeshCount < 1)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    CMeshData* render_data = pMeshScene;
    if (!render_data)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    if (!render_data->pSceneAnimatableObject)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    auto render_ent = render_data->pSceneAnimatableObject->Owner();
    if (!render_ent.valid())
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    auto entity = I::GameEntity->Instance->Get(render_ent);
    if (!entity)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    const auto target = GetTargetType(entity);

    if (target == ChamsEntity::VIEWMODEL && config.viewmodel) {
        pMeshScene->pMaterial = GetMaterial(config.material, false);
        pMeshScene->color.r = static_cast<uint8_t>(config.viewmodelColor.r * 255.0f);
        pMeshScene->color.g = static_cast<uint8_t>(config.viewmodelColor.g * 255.0f);
        pMeshScene->color.b = static_cast<uint8_t>(config.viewmodelColor.b * 255.0f);
        pMeshScene->color.a = static_cast<uint8_t>(config.viewmodelColor.a * 255.0f);
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    }

    if (target == ChamsEntity::HANDS && config.arms) {
        pMeshScene->pMaterial = GetMaterial(config.material, false);
        pMeshScene->color.r = static_cast<uint8_t>(config.armColor.r * 255.0f);
        pMeshScene->color.g = static_cast<uint8_t>(config.armColor.g * 255.0f);
        pMeshScene->color.b = static_cast<uint8_t>(config.armColor.b * 255.0f);
        pMeshScene->color.a = static_cast<uint8_t>(config.armColor.a * 255.0f);
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    }

    if (target == ChamsEntity::LOCAL && config.local)
    {
        pMeshScene->pMaterial = GetMaterial(config.material, false);
        pMeshScene->color.r = static_cast<uint8_t>(config.localColor.r * 255.0f);
        pMeshScene->color.g = static_cast<uint8_t>(config.localColor.g * 255.0f);
        pMeshScene->color.b = static_cast<uint8_t>(config.localColor.b * 255.0f);
        pMeshScene->color.a = static_cast<uint8_t>(config.localColor.a * 255.0f);
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    }

    if (target == TEAM)
    {
        if (config.teamInvisible)
        {
            pMeshScene->pMaterial = GetMaterial(config.material, true);
            pMeshScene->color.r = static_cast<uint8_t>(config.teamInvisibleColor.r * 255.0f);
            pMeshScene->color.g = static_cast<uint8_t>(config.teamInvisibleColor.g * 255.0f);
            pMeshScene->color.b = static_cast<uint8_t>(config.teamInvisibleColor.b * 255.0f);
            pMeshScene->color.a = static_cast<uint8_t>(config.teamInvisibleColor.a * 255.0f);
            original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        }
        if (config.teamVisible)
        {
            pMeshScene->pMaterial = GetMaterial(config.material, false);
            pMeshScene->color.r = static_cast<uint8_t>(config.teamColor.r * 255.0f);
            pMeshScene->color.g = static_cast<uint8_t>(config.teamColor.g * 255.0f);
            pMeshScene->color.b = static_cast<uint8_t>(config.teamColor.b * 255.0f);
            pMeshScene->color.a = static_cast<uint8_t>(config.teamColor.a * 255.0f);
            return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        }
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    }

    if (target != ENEMY)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    bool og = !config.enemyVisible && !config.enemyInvisible;
    if (og)
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);

    if (config.enemyInvisible) {
        pMeshScene->pMaterial = GetMaterial(config.material, true);
        pMeshScene->color.r = static_cast<uint8_t>(config.enemyInvisibleColor.r * 255.0f);
        pMeshScene->color.g = static_cast<uint8_t>(config.enemyInvisibleColor.g * 255.0f);
        pMeshScene->color.b = static_cast<uint8_t>(config.enemyInvisibleColor.b * 255.0f);
        pMeshScene->color.a = static_cast<uint8_t>(config.enemyInvisibleColor.a * 255.0f);

        original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    }

    if (config.enemyVisible) {
        pMeshScene->pMaterial = GetMaterial(config.material, false);
        pMeshScene->color.r = static_cast<uint8_t>(config.enemyColor.r * 255.0f);
        pMeshScene->color.g = static_cast<uint8_t>(config.enemyColor.g * 255.0f);
        pMeshScene->color.b = static_cast<uint8_t>(config.enemyColor.b * 255.0f);
        pMeshScene->color.a = static_cast<uint8_t>(config.enemyColor.a * 255.0f);
        return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    }

    return original(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
}

CMaterial2* GetMaterialPtr(const int type, const bool invisible) noexcept
{
	return GetMaterial(type, invisible);
}

void chams::cacheChamsTargets()
{
	cachedOwnerCount = 0;

	C_CSPlayerPawn* localPawn = g_ctx ? g_ctx->local_pawn : nullptr;
	if (!localPawn || !I::GameEntity || !I::GameEntity->Instance)
		return;

	// Local player pawn
	if (auto* localEnt = static_cast<C_BaseEntity*>(localPawn))
	{
		const CBaseHandle h = localEnt->handle();
		if (h.valid() && cachedOwnerCount < 128)
		{
			cachedOwnerIndices[cachedOwnerCount] = h.index();
			cachedOwnerTypes[cachedOwnerCount] = LOCAL;
			++cachedOwnerCount;
		}
	}

	// Active weapon (viewmodel)
	if (auto* activeWeapon = localPawn->GetActiveWeapon())
	{
		const CBaseHandle h = static_cast<C_BaseEntity*>(activeWeapon)->handle();
		if (h.valid() && cachedOwnerCount < 128)
		{
			cachedOwnerIndices[cachedOwnerCount] = h.index();
			cachedOwnerTypes[cachedOwnerCount] = VIEWMODEL;
			++cachedOwnerCount;
		}
	}

	// Arms / Viewmodel HUD models
	for (const char* className : { "C_CS2HudModelArms", "C_CS2HudModelWeapon", "C_BaseViewModel" })
	{
		for (auto* entity : g_entitySystem->get(className))
		{
			if (!entity || cachedOwnerCount >= 128) break;
			const uint32_t idx = entity->get_entity_by_handle() & 0x7FFF;
			if (idx != 0 && idx != 0x7FFF)
			{
				cachedOwnerIndices[cachedOwnerCount] = idx;
				cachedOwnerTypes[cachedOwnerCount] = (std::string_view(className) == "C_CS2HudModelArms") ? HANDS : VIEWMODEL;
				++cachedOwnerCount;
			}
		}
	}

	// Players
	auto entities = g_entitySystem->get("CCSPlayerController");
	for (auto* entity : entities)
	{
		if (!entity || cachedOwnerCount >= 128) break;
		auto* controller = reinterpret_cast<CCSPlayerController*>(entity);
		if (!controller || controller == g_ctx->local_controller) continue;
		if (!controller->m_hPawn().valid()) continue;
		auto* player = I::GameEntity->Instance->Get<C_CSPlayerPawn>(controller->m_hPawn());
		if (!player || player->m_iHealth() <= 0) continue;
		const CBaseHandle h = static_cast<C_BaseEntity*>(player)->handle();
		if (!h.valid()) continue;
		cachedOwnerIndices[cachedOwnerCount] = h.index();
		cachedOwnerTypes[cachedOwnerCount] = (player->m_iTeamNum() == localPawn->m_iTeamNum()) ? TEAM : ENEMY;
		++cachedOwnerCount;
	}
}

void __fastcall chams::hkGeneratePrimitives(void* a1, void* Object, void* a3, CMeshPrimitiveOutputBuffer* RenderBuffer)
{
	Runtime::CallbackGuard callback;
	if (Runtime::IsShuttingDown() || !callback.FeatureAdmitted())
		return H::GeneratePrimitives.GetOriginal()(a1, Object, a3, RenderBuffer);

	// Preview observation shares the existing scene hook. It samples the game-owned
	// portrait panel and never installs a second GeneratePrimitives detour.
	tw::model_preview::onGeneratePrimitives();

	if (!Object || !RenderBuffer)
		return H::GeneratePrimitives.GetOriginal()(a1, Object, a3, RenderBuffer);
	const bool previewObject = tw::model_preview::isPreviewSceneObject(Object);

	if (!Config::visual_chams.enemyChams && !Config::visual_chams.enemyChamsInvisible &&
		!Config::visual_chams.teamChams && !Config::visual_chams.teamChamsInvisible &&
		!Config::visual_chams.localChams && !Config::visual_chams.viewmodelChams &&
		!Config::visual_chams.armChams)
		return H::GeneratePrimitives.GetOriginal()(a1, Object, a3, RenderBuffer);

	const std::uint32_t ownerHandle = *reinterpret_cast<const std::uint32_t*>(
		reinterpret_cast<const std::uintptr_t>(Object) + 0xC0);
	if (!ownerHandle || ownerHandle == 0xFFFFFFFF)
		return H::GeneratePrimitives.GetOriginal()(a1, Object, a3, RenderBuffer);

	const std::uint32_t ownerIndex = ownerHandle & 0x7FFF;

	ChamsEntity target = previewObject ? ENEMY : INVALID;
	for (int idx = 0; idx < cachedOwnerCount; ++idx)
	{
		if (previewObject)
			break;
		if (cachedOwnerIndices[idx] == ownerIndex)
		{
			target = cachedOwnerTypes[idx];
			break;
		}
	}
	if (target == INVALID)
		return H::GeneratePrimitives.GetOriginal()(a1, Object, a3, RenderBuffer);

	// Glow only. Its two variants are both additive and translucent, so they
	// land in the same render bucket and the second layer composites over the
	// first -- which is what makes the per-pixel split work. Flat and Illuminate
	// were tried and could not be made to do it, so they are not offered.
	constexpr int matType = GLOW;

	bool wantOccluded = false;
	bool wantVisible = false;
	const ImVec4* occludedColor = nullptr;
	const ImVec4* visibleColor = nullptr;

	switch (target)
	{
	// Both layers, every player, no visibility test. The depth-ignoring layer
	// paints the whole silhouette and the depth-tested one composites over the
	// parts in view, so the split falls out of the depth buffer per pixel.
	case ENEMY:
		wantOccluded = Config::visual_chams.enemyChamsInvisible;
		wantVisible = Config::visual_chams.enemyChams;
		occludedColor = &Config::visual_chams.colVisualChamsIgnoreZ;
		visibleColor = &Config::visual_chams.colVisualChams;
		break;
	case TEAM:
		wantOccluded = Config::visual_chams.teamChamsInvisible;
		wantVisible = Config::visual_chams.teamChams;
		occludedColor = &Config::visual_chams.teamcolVisualChamsIgnoreZ;
		visibleColor = &Config::visual_chams.teamcolVisualChams;
		break;
	case LOCAL:
		wantVisible = Config::visual_chams.localChams;
		visibleColor = &Config::visual_chams.localcolChams;
		break;
	case VIEWMODEL:
		wantVisible = Config::visual_chams.viewmodelChams;
		visibleColor = &Config::visual_chams.colViewmodelChams;
		break;
	case HANDS:
		wantVisible = Config::visual_chams.armChams;
		visibleColor = &Config::visual_chams.colArmChams;
		break;
	default: break;
	}

	if (!wantOccluded && !wantVisible)
		return H::GeneratePrimitives.GetOriginal()(a1, Object, a3, RenderBuffer);

	// Disable cull flag so backfaces and occluded geometry render
	*reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(Object) + 0x78) &= ~(1u << 3);

	// Each pass appends a fresh batch of primitives and stamps its own material
	// and tint on exactly that batch. Two batches is what produces two colours:
	// the through-walls batch ignores depth and paints the whole silhouette,
	// then the depth-tested batch paints over the parts that really are visible.
	// The split is per pixel, which is why it cannot be done by picking one
	// colour per player.
	//
	// Returns false when the pass produced no primitives of its own, which means
	// nothing was stamped and the model draws with the game's material.
	// Emitting and stamping are separated so the stride can be measured before
	// any of our own materials overwrite the game's.
	//
	// The stride cannot be read inside one batch: the primitives of a player are
	// different mesh parts carrying different materials, so there is no repeat to
	// find. What does repeat is the batch itself -- the same object generated
	// twice produces the same sequence of materials -- so the offset at which
	// batch one's first material reappears is count * stride.
	struct PendingBatch final
	{
		int start{};
		int count{};
		CMaterial2* material{};
		std::uint32_t tint{};
	};
	PendingBatch pending[4]{};
	int pendingCount = 0;

	const auto emitPass = [&](bool invisible, int materialType, const ImVec4* colorSrc) -> bool
	{
		CMaterial2* mat = GetMaterial(materialType, invisible);
		if (!mat || !colorSrc || !RenderBuffer->m_pOut)
			return false;

		const std::uint32_t tintColor =
			(static_cast<std::uint32_t>(static_cast<std::uint8_t>(colorSrc->x * 255.0f))) |
			(static_cast<std::uint32_t>(static_cast<std::uint8_t>(colorSrc->y * 255.0f)) << 8) |
			(static_cast<std::uint32_t>(static_cast<std::uint8_t>(colorSrc->z * 255.0f)) << 16) |
			(static_cast<std::uint32_t>(static_cast<std::uint8_t>(colorSrc->w * 255.0f)) << 24);

		const int oldCount = RenderBuffer->m_nStartPrimitives;
		if (oldCount >= RenderBuffer->m_nMaximumOutputPrimitives)
			return false;

		H::GeneratePrimitives.GetOriginal()(a1, Object, a3, RenderBuffer);

		const int newCount = RenderBuffer->m_nStartPrimitives;
		if (newCount <= oldCount)
			return false;

		if (pendingCount < 4)
			pending[pendingCount++] = { oldCount, newCount - oldCount, mat, tintColor };
		return true;
	};

	// Only ever measured from an object that starts at index 0, because locating
	// any later batch would already require knowing the stride.
	const auto resolveStride = [&]() -> std::size_t
	{
		static std::size_t cached = 0;
		if (cached != 0)
			return cached;
		if (pendingCount < 2 || pending[0].start != 0 || pending[0].count < 1 || !RenderBuffer->m_pOut)
			return sizeof(ChamsPrimitive);

		const auto* base = reinterpret_cast<const std::uint8_t*>(RenderBuffer->m_pOut);
		const auto marker = *reinterpret_cast<const std::uintptr_t*>(base + kPrimitiveMaterialOffset);
		if (marker == 0)
			return sizeof(ChamsPrimitive);

		const auto count = static_cast<std::size_t>(pending[0].count);
		std::size_t stride = 0;
		for (std::size_t offset = 8; offset <= 0x1000; offset += 8)
		{
			if (*reinterpret_cast<const std::uintptr_t*>(base + kPrimitiveMaterialOffset + offset) != marker)
				continue;
			if (offset % count != 0)
				continue;
			const std::size_t candidate = offset / count;
			// A primitive is neither tiny nor enormous; this rejects an
			// unrelated pointer that happens to match the same value.
			if (candidate >= 0x40 && candidate <= 0x100 && candidate % 8 == 0)
			{
				stride = candidate;
				break;
			}
		}

		cached = stride != 0 ? stride : sizeof(ChamsPrimitive);
		return cached;
	};

	const auto stampPending = [&]()
	{
		if (pendingCount == 0)
			return;
		const std::size_t stride = resolveStride();
		auto* const bytes = reinterpret_cast<std::uint8_t*>(RenderBuffer->m_pOut);
		for (int b = 0; b < pendingCount; ++b)
		{
			const PendingBatch& batch = pending[b];
			for (int i = batch.start; i < batch.start + batch.count; ++i)
			{
				auto* const primitive = bytes + static_cast<std::size_t>(i) * stride;
				// Material and tint only. The second pointer this used to write
				// sits at an offset from a struct layout that is no longer
				// trusted, and velocity's working implementation never sets it.
				*reinterpret_cast<CMaterial2**>(primitive + kPrimitiveMaterialOffset) = batch.material;
				*reinterpret_cast<std::uint32_t*>(primitive + kPrimitiveTintOffset) = batch.tint;
			}
		}
	};

	const ImVec4 blackBody{ 0.0f, 0.0f, 0.0f, 1.0f };
	const auto renderLayer = [&](bool invisible, const ImVec4* color) -> bool
	{
		if (matType == GLOW)
		{
			// The effects material is additive and intentionally translucent. Give
			// it an opaque black silhouette first so Fresnel remains an outline
			// instead of making the entire player look transparent.
			emitPass(invisible, FLAT, &blackBody);
		}
		return emitPass(invisible, matType, color);
	};

	// Through-walls first, visible second, so the depth-tested layer composites
	// on top of it.
	if (wantOccluded)
		renderLayer(true, occludedColor);

	if (wantVisible)
		renderLayer(false, visibleColor);

	// Every batch is stamped here, after all of them exist.
	stampPending();

}
