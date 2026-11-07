#include "model_preview.h"

#include "../../hooks/includeHooks.h"
#include "../../config/config.h"
#include "../../runtime/runtime.h"
#include "../../utils/memory/patternscan/patternscan.h"
#include "../../utils/module/module.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using RunScriptFn = void(__fastcall*)(void*, void*, const char*, const char*, std::uint64_t, bool);
	using GetResourceViewFn = ID3D11ShaderResourceView*(__fastcall*)(
		void*, void*, std::int8_t, bool, const char*);
	using CreatePlayerPanelFn = void*(__fastcall*)(void*, void*);
	using DestroyPlayerPanelFn = void*(__fastcall*)(void*, unsigned int);
	using GetBonePanelPositionFn = void*(__fastcall*)(void*, float*, const char*);

	constexpr char kTextureMarker[] = "templeware_model_preview";

	// Properties and panel type are taken from this build's panorama/scripts/inspect.vts_c.
	constexpr char kCreateScript[] = R"JS((function () {
var root = $.GetContextPanel();
while (root && root.GetParent()) root = root.GetParent();
if (!root) return;
var old = root.FindChildTraverse('TempleWareModelPreviewRoot');
if (old) old.DeleteAsync(0.0);
var host = $.CreatePanel('Panel', root, 'TempleWareModelPreviewRoot', {
    hittest: false,
    style: 'width: 512px; height: 512px; opacity: 0.01; brightness: 0.0; wash-color: #00000000; position: 0px 0px 0px; z-index: -99999;'
});
if (!host) return;
var sequence = 'ct_main_menu_rifle_awp_lookat';
var panel = $.CreatePanel('MapPlayerPreviewPanel', host, 'TempleWareModelPreview', {
    map: 'ui/buy_menu', camera: 'cam_loadoutmenu_ct',
    'require-composition-layer': true,
    'composition-layer-texture-name': 'templeware_model_preview',
    playermodel: 'agents/models/ctm_st6/ctm_st6_variante.vmdl',
    playername: 'vanity_character', animgraphcharactermode: 'main-menu',
    pose_sequence: sequence, player: true, mouse_rotate: true,
    sync_spawn_addons: true, 'transparent-background': true,
    'pin-fov': 'vertical', hittest: false,
    style: 'width: 100%; height: 100%; opacity: 1.0;'
});
if (!panel) return;
try {
    if (panel.EquipPlayerWithItem && typeof BigInt === 'function')
        panel.EquipPlayerWithItem(BigInt('0xF000000000000000') | BigInt(9));
} catch (e) {}
$.Schedule(0.25, function () {
    try {
        if (!panel || (panel.IsValid && !panel.IsValid())) return;
        if (panel.SetReadyForDisplay) panel.SetReadyForDisplay(true);
        if (panel.PlaySequence) panel.PlaySequence(sequence);
    } catch (e) {}
});
var keepAliveFlip = false;
var keepAlive = function () {
    try {
        if (!panel || (panel.IsValid && !panel.IsValid())) return;
        if (panel.SetReadyForDisplay) panel.SetReadyForDisplay(true);
        keepAliveFlip = !keepAliveFlip;
        panel.style.opacity = keepAliveFlip ? '1.0' : '0.999';
        $.Schedule(0.25, keepAlive);
    } catch (e) {}
};
$.Schedule(0.25, keepAlive);
})();)JS";

	constexpr char kCleanupScript[] = R"JS((function () {
var root = $.GetContextPanel();
while (root && root.GetParent()) root = root.GetParent();
if (!root) return;
var panel = root.FindChildTraverse('TempleWareModelPreviewRoot');
if (panel) panel.DeleteAsync(0.0);
})();)JS";

	CInlineHookObj<RunScriptFn> g_runScriptHook;
	CInlineHookObj<GetResourceViewFn> g_resourceViewHook;
	CInlineHookObj<CreatePlayerPanelFn> g_createPlayerPanelHook;
	CInlineHookObj<DestroyPlayerPanelFn> g_destroyPlayerPanelHook;
	GetBonePanelPositionFn g_getBonePanelPosition = nullptr;
	void* g_playerPanelVtable = nullptr;
	std::atomic<void*> g_playerPanel{ nullptr };
	std::atomic<ULONGLONG> g_capturePanelUntil{ 0 };
	std::atomic<ULONGLONG> g_lastBoneSample{ 0 };
	std::atomic<ULONGLONG> g_lastPreviewDraw{ 0 };
	std::atomic<ULONGLONG> g_lastPanelDiagnostic{ 0 };
	std::atomic_uint g_panelSampleFailures{ 0 };
	std::atomic<tw::model_preview::Status> g_status{ tw::model_preview::Status::Unavailable };
	std::atomic_bool g_cleanupComplete{ true };
	std::mutex g_stateMutex;
	bool g_creationRequested = false;
	bool g_cleanupRequested = false;
	bool g_panelSubmitted = false;
	ULONGLONG g_lastSubmission = 0;
	std::mutex g_textureMutex;
	ID3D11ShaderResourceView* g_texture = nullptr;
	std::vector<ID3D11ShaderResourceView*> g_retiredTextures;
	std::atomic<float> g_textureAspect{ 1.0f };
	std::recursive_mutex g_panelLifetimeMutex;
	std::array<void*, 64> g_candidatePanels{};
	std::size_t g_candidatePanelCount = 0;

	constexpr const char* kBoneNames[] = {
		"head_0", "neck_0", "spine_3", "spine_2", "spine_1", "spine_0", "pelvis",
		"arm_upper_L", "arm_lower_L", "hand_L", "arm_upper_R", "arm_lower_R", "hand_R",
		"leg_upper_L", "leg_lower_L", "ankle_L", "leg_upper_R", "leg_lower_R", "ankle_R"
	};
	constexpr int kBoneCount = static_cast<int>(std::size(kBoneNames));
	constexpr int kConnections[][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 },
		{ 2, 7 }, { 7, 8 }, { 8, 9 }, { 2, 10 }, { 10, 11 }, { 11, 12 },
		{ 6, 13 }, { 13, 14 }, { 14, 15 }, { 6, 16 }, { 16, 17 }, { 17, 18 }
	};

	struct BoneSnapshot
	{
		float uv[kBoneCount][2]{};
		bool valid[kBoneCount]{};
		ULONGLONG sampledAt{};
	};

	struct PanelProbe
	{
		bool readable{};
		bool expectedVtable{};
		int currentSlot{ -1 };
		int slotCount{};
		std::uintptr_t slots{};
		std::uint32_t previewHandle{};
	};

	std::mutex g_boneMutex;
	BoneSnapshot g_bones;

	void* resolveRipTarget(const std::uint8_t* instruction, std::size_t displacementOffset,
		std::size_t instructionSize) noexcept
	{
		if (instruction == nullptr)
			return nullptr;
		const auto displacement = *reinterpret_cast<const std::int32_t*>(instruction + displacementOffset);
		return const_cast<std::uint8_t*>(instruction + instructionSize + displacement);
	}

	bool pointIsFinite(const float point[2]) noexcept
	{
		return std::isfinite(point[0]) && std::isfinite(point[1]) &&
			point[0] >= 0.0f && point[0] <= 1.0f && point[1] >= 0.0f && point[1] <= 1.0f;
	}

	bool samplePanelUnsafe(void* panel, BoneSnapshot* snapshot) noexcept
	{
		if (panel == nullptr || snapshot == nullptr || g_getBonePanelPosition == nullptr ||
			*reinterpret_cast<void**>(panel) != g_playerPanelVtable)
		{
			return false;
		}

		float minX = 1.0f;
		float maxX = 0.0f;
		float minY = 1.0f;
		float maxY = 0.0f;
		int validCount = 0;
		for (int index = 0; index < kBoneCount; ++index)
		{
			float output[2]{};
			g_getBonePanelPosition(panel, output, kBoneNames[index]);
			snapshot->valid[index] = pointIsFinite(output);
			if (!snapshot->valid[index])
				continue;
			snapshot->uv[index][0] = output[0];
			snapshot->uv[index][1] = output[1];
			minX = (std::min)(minX, output[0]);
			maxX = (std::max)(maxX, output[0]);
			minY = (std::min)(minY, output[1]);
			maxY = (std::max)(maxY, output[1]);
			++validCount;
		}
		snapshot->sampledAt = GetTickCount64();

		// Bone availability depends on the model and active animation. Selection
		// only needs enough stable anchors for a body box; the renderer already
		// skips individual missing bones and their connections.
		const bool torsoValid = snapshot->valid[0] && snapshot->valid[2] && snapshot->valid[6];
		const bool leftLegValid = snapshot->valid[13] && snapshot->valid[14] && snapshot->valid[15];
		const bool rightLegValid = snapshot->valid[16] && snapshot->valid[17] && snapshot->valid[18];
		if (validCount < 12 || !torsoValid || (!leftLegValid && !rightLegValid))
			return false;

		const int ankleIndex = leftLegValid ? 15 : 18;
		return maxX - minX > 0.08f && maxY - minY > 0.20f &&
			snapshot->uv[0][1] + 0.10f < snapshot->uv[ankleIndex][1];
	}

	bool samplePanelGuarded(void* panel, BoneSnapshot* snapshot) noexcept
	{
		__try
		{
			return samplePanelUnsafe(panel, snapshot);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	bool panelHasExpectedVtable(void* panel) noexcept
	{
		__try
		{
			return panel != nullptr && *reinterpret_cast<void**>(panel) == g_playerPanelVtable;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	PanelProbe probePanel(void* panel) noexcept
	{
		PanelProbe probe{};
		__try
		{
			probe.expectedVtable = panel != nullptr &&
				*reinterpret_cast<void**>(panel) == g_playerPanelVtable;
			if (!probe.expectedVtable)
				return probe;
			probe.currentSlot = *reinterpret_cast<const int*>(
				reinterpret_cast<std::uintptr_t>(panel) + 0x8CC);
			probe.slotCount = *reinterpret_cast<const int*>(
				reinterpret_cast<std::uintptr_t>(panel) + 0x8D0);
			probe.slots = *reinterpret_cast<const std::uintptr_t*>(
				reinterpret_cast<std::uintptr_t>(panel) + 0x8D8);
			if (probe.currentSlot >= 0 && probe.currentSlot < probe.slotCount && probe.slots != 0)
			{
				probe.previewHandle = *reinterpret_cast<const std::uint32_t*>(
					probe.slots + static_cast<std::size_t>(probe.currentSlot) * 0x98);
			}
			probe.readable = true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			probe.readable = false;
		}
		return probe;
	}

	bool sceneObjectMatchesPanelUnsafe(void* panel, void* sceneObject) noexcept
	{
		if (*reinterpret_cast<void**>(panel) != g_playerPanelVtable)
			return false;
		const int currentSlot = *reinterpret_cast<const int*>(
			reinterpret_cast<std::uintptr_t>(panel) + 0x8CC);
		const int slotCount = *reinterpret_cast<const int*>(
			reinterpret_cast<std::uintptr_t>(panel) + 0x8D0);
		const auto slots = *reinterpret_cast<const std::uintptr_t*>(
			reinterpret_cast<std::uintptr_t>(panel) + 0x8D8);
		if (currentSlot < 0 || currentSlot >= slotCount || slots == 0)
			return false;
		const auto previewHandle = *reinterpret_cast<const std::uint32_t*>(
			slots + static_cast<std::size_t>(currentSlot) * 0x98);
		const auto ownerHandle = *reinterpret_cast<const std::uint32_t*>(
			reinterpret_cast<std::uintptr_t>(sceneObject) + 0xC0);
		return previewHandle != 0 && previewHandle != 0xFFFFFFFF &&
			(ownerHandle & 0x7FFF) == (previewHandle & 0x7FFF);
	}

	bool sceneObjectMatchesPanelGuarded(void* panel, void* sceneObject) noexcept
	{
		__try
		{
			return sceneObjectMatchesPanelUnsafe(panel, sceneObject);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	bool addCandidatePanel(void* panel) noexcept
	{
		for (std::size_t index = 0; index < g_candidatePanelCount; ++index)
		{
			if (g_candidatePanels[index] == panel)
				return false;
		}
		if (g_candidatePanelCount < g_candidatePanels.size())
		{
			g_candidatePanels[g_candidatePanelCount++] = panel;
			return true;
		}
		return false;
	}

	void removeCandidatePanel(void* panel) noexcept
	{
		for (std::size_t index = 0; index < g_candidatePanelCount; ++index)
		{
			if (g_candidatePanels[index] != panel)
				continue;
			g_candidatePanels[index] = g_candidatePanels[g_candidatePanelCount - 1];
			g_candidatePanels[g_candidatePanelCount - 1] = nullptr;
			--g_candidatePanelCount;
			return;
		}
	}

	void sampleNativePanel() noexcept
	{
		if (g_getBonePanelPosition == nullptr)
			return;

		const ULONGLONG now = GetTickCount64();
		if (now - g_lastPreviewDraw.load(std::memory_order_acquire) >= 250)
			return;
		ULONGLONG previous = g_lastBoneSample.load(std::memory_order_acquire);
		if (now - previous < 16 ||
			!g_lastBoneSample.compare_exchange_strong(previous, now, std::memory_order_acq_rel))
		{
			return;
		}

		void* panel = g_playerPanel.load(std::memory_order_acquire);
		BoneSnapshot snapshot{};
		{
			std::scoped_lock lifetimeLock(g_panelLifetimeMutex);
			if (panel == nullptr)
			{
				for (std::size_t index = 0; index < g_candidatePanelCount; ++index)
				{
					void* candidate = g_candidatePanels[index];
					BoneSnapshot candidateSnapshot{};
					if (!samplePanelGuarded(candidate, &candidateSnapshot))
						continue;
					panel = candidate;
					snapshot = candidateSnapshot;
					g_playerPanel.store(panel, std::memory_order_release);
					g_panelSampleFailures.store(0, std::memory_order_release);
					g_capturePanelUntil.store(0, std::memory_order_release);
					std::cout << "  model preview: native panel skeleton selected" << std::endl;
					break;
				}
				if (panel == nullptr)
				{
					ULONGLONG diagnostic = g_lastPanelDiagnostic.load(std::memory_order_acquire);
					if (now - diagnostic >= 2000 &&
						g_lastPanelDiagnostic.compare_exchange_strong(
							diagnostic, now, std::memory_order_acq_rel))
					{
						std::cout << "  model preview: waiting for native skeleton (candidates="
							<< g_candidatePanelCount << ")" << std::endl;
						for (std::size_t index = 0; index < g_candidatePanelCount; ++index)
						{
							BoneSnapshot candidateSnapshot{};
							(void)samplePanelGuarded(g_candidatePanels[index], &candidateSnapshot);
							int validBones = 0;
							for (bool valid : candidateSnapshot.valid)
								validBones += valid ? 1 : 0;
							const PanelProbe probe = probePanel(g_candidatePanels[index]);
							std::cout << "    candidate " << index
								<< ": readable=" << (probe.readable ? 1 : 0)
								<< " vtable=" << (probe.expectedVtable ? 1 : 0)
								<< " slot=" << probe.currentSlot << '/' << probe.slotCount
								<< " slots=0x" << std::hex << probe.slots
								<< " handle=0x" << probe.previewHandle << std::dec
								<< " bones=" << validBones
								<< " head=(" << candidateSnapshot.uv[0][0] << ','
								<< candidateSnapshot.uv[0][1] << ')'
								<< " pelvis=(" << candidateSnapshot.uv[6][0] << ','
								<< candidateSnapshot.uv[6][1] << ')'
								<< std::endl;
						}
					}
					return;
				}
			}
			else if (g_playerPanel.load(std::memory_order_acquire) != panel)
			{
				return;
			}
			else if (!samplePanelGuarded(panel, &snapshot))
			{
				if (g_panelSampleFailures.fetch_add(1, std::memory_order_acq_rel) >= 30)
				{
					g_playerPanel.store(nullptr, std::memory_order_release);
					g_panelSampleFailures.store(0, std::memory_order_release);
				}
				return;
			}
			else
			{
				g_panelSampleFailures.store(0, std::memory_order_release);
			}
		}

		std::scoped_lock lock(g_boneMutex);
		g_bones = snapshot;
	}

	void* __fastcall hkCreatePlayerPanel(void* factory, void* panoramaPanel)
	{
		Runtime::CallbackGuard callback;
		void* panel = g_createPlayerPanelHook.GetOriginal()(factory, panoramaPanel);
		if (!Runtime::IsShuttingDown() && panel != nullptr)
		{
			std::scoped_lock lock(g_panelLifetimeMutex);
			if (!panelHasExpectedVtable(panel))
				return panel;
			// Construction is asynchronous and may complete after the submission
			// window, especially after reinjection while an older composition texture
			// is still alive. Keep every panel with the verified native vtable; the
			// bone probe selects the candidate that owns an active preview entity.
			if (addCandidatePanel(panel))
				std::cout << "  model preview: native panel candidate captured" << std::endl;
		}
		return panel;
	}

	void* __fastcall hkDestroyPlayerPanel(void* panel, unsigned int flags)
	{
		Runtime::CallbackGuard callback;
		bool ownedPanel = false;
		{
			// Destruction cannot begin while a render callback is using the panel helper.
			std::scoped_lock lock(g_panelLifetimeMutex);
			void* expected = panel;
			ownedPanel = g_playerPanel.compare_exchange_strong(
				expected, nullptr, std::memory_order_acq_rel);
			removeCandidatePanel(panel);
		}
		if (ownedPanel && !Runtime::IsShuttingDown())
		{
			std::scoped_lock lock(g_stateMutex);
			g_panelSubmitted = false;
			g_creationRequested = true;
			g_lastSubmission = 0;
			g_status.store(tw::model_preview::Status::AwaitingPanorama,
				std::memory_order_release);
			std::scoped_lock boneLock(g_boneMutex);
			g_bones = {};
		}
		return g_destroyPlayerPanelHook.GetOriginal()(panel, flags);
	}

	void* resolveGetResourceView() noexcept
	{
		const auto base = modules.getModule("rendersystemdx11");
		if (base == 0)
			return nullptr;

		const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
			return nullptr;
		const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
			return nullptr;

		constexpr unsigned char prologue[] = {
			0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18,
			0x48, 0x89, 0x7C, 0x24, 0x20, 0x48, 0x89, 0x4C, 0x24, 0x08,
			0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48,
			0x8D, 0x6C, 0x24, 0xE0, 0x48, 0x81, 0xEC, 0x20, 0x01, 0x00,
			0x00, 0x33, 0xFF, 0x4D, 0x0F, 0xBE, 0xF8, 0x89, 0x7D, 0x50,
			0x45, 0x0F, 0xB6, 0xE1, 0x4C, 0x8B, 0x2D
		};
		constexpr std::string_view exactName = "CTextureManagerDx11::GetResourceView";
		constexpr std::string_view mipName = "GetResourceViewForMipLevel";
		const std::size_t imageSize = nt->OptionalHeader.SizeOfImage;
		const auto image = reinterpret_cast<const unsigned char*>(base);
		void* resolved = nullptr;
		int resolvedCount = 0;

		for (std::size_t offset = 0; offset + sizeof(prologue) < imageSize; ++offset)
		{
			if (std::memcmp(image + offset, prologue, sizeof(prologue)) != 0)
				continue;

			bool identifiesWholeResource = false;
			const std::size_t functionBytes = std::min<std::size_t>(0x600, imageSize - offset);
			for (std::size_t index = 0; index + sizeof(std::int32_t) <= functionBytes; ++index)
			{
				const auto displacement = *reinterpret_cast<const std::int32_t*>(image + offset + index);
				const auto target = static_cast<std::intptr_t>(base + offset + index + sizeof(displacement)) + displacement;
				if (target < static_cast<std::intptr_t>(base) ||
					target + static_cast<std::intptr_t>(exactName.size() + mipName.size()) >=
						static_cast<std::intptr_t>(base + imageSize))
				{
					continue;
				}

				const char* text = reinterpret_cast<const char*>(target);
				const void* terminator = std::memchr(text, '\0', 160);
				if (terminator == nullptr)
					continue;
				const std::string_view diagnostic(text,
					static_cast<const char*>(terminator) - text);
				if (diagnostic.find(exactName) != std::string_view::npos &&
					diagnostic.find(mipName) == std::string_view::npos)
				{
					identifiesWholeResource = true;
					break;
				}
			}

			if (identifiesWholeResource)
			{
				resolved = const_cast<unsigned char*>(image + offset);
				++resolvedCount;
			}
		}

		return resolvedCount == 1 ? resolved : nullptr;
	}

	bool isPreviewBinding(void* binding) noexcept
	{
		bool matches = false;
		__try
		{
			if (binding != nullptr)
			{
				const auto holder = *reinterpret_cast<const char***>(
					reinterpret_cast<std::uintptr_t>(binding) + 0x08);
				if (holder != nullptr && *holder != nullptr)
					matches = std::strstr(*holder, kTextureMarker) != nullptr;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			matches = false;
		}
		return matches;
	}

	bool validPreviewTexture(ID3D11ShaderResourceView* view, float* aspect) noexcept
	{
		if (view == nullptr)
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
		view->GetDesc(&viewDescription);
		if (viewDescription.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D)
			return false;

		ID3D11Resource* resource = nullptr;
		view->GetResource(&resource);
		if (resource == nullptr)
			return false;

		ID3D11Texture2D* texture = nullptr;
		const HRESULT result = resource->QueryInterface(__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&texture));
		resource->Release();
		if (FAILED(result) || texture == nullptr)
			return false;

		D3D11_TEXTURE2D_DESC textureDescription{};
		texture->GetDesc(&textureDescription);
		texture->Release();
		if (textureDescription.Width < 128 || textureDescription.Height < 128)
			return false;
		if (aspect != nullptr)
			*aspect = static_cast<float>(textureDescription.Width) /
				static_cast<float>(textureDescription.Height);
		return true;
	}

	void __fastcall hkRunScript(void* engine, void* contextPanel, const char* script,
		const char* sourceName, std::uint64_t sourcePosition, bool unknownFlag)
	{
		Runtime::CallbackGuard callback;
		const auto original = g_runScriptHook.GetOriginal();
		original(engine, contextPanel, script, sourceName, sourcePosition, unknownFlag);

		if (engine == nullptr || contextPanel == nullptr)
			return;

		bool cleanup = false;
		bool create = false;
		{
			std::scoped_lock lock(g_stateMutex);
			if (g_cleanupRequested)
			{
				g_cleanupRequested = false;
				cleanup = g_panelSubmitted;
				g_panelSubmitted = false;
				g_creationRequested = false;
			}
			else if (!Runtime::IsShuttingDown() && g_creationRequested)
			{
				g_creationRequested = false;
				create = !g_panelSubmitted || GetTickCount64() - g_lastSubmission >= 5000;
				if (create)
				{
					g_panelSubmitted = true;
					g_lastSubmission = GetTickCount64();
				}
			}
		}

		if (cleanup)
		{
			original(engine, contextPanel, kCleanupScript, "TempleWareModelPreviewCleanup", 0, true);
			g_playerPanel.store(nullptr, std::memory_order_release);
			{
				std::scoped_lock lock(g_boneMutex);
				g_bones = {};
			}
			g_cleanupComplete.store(true, std::memory_order_release);
			return;
		}

		if (!create)
			return;

		// MapPlayerPreviewPanel's native client and preview entity initialize
		// asynchronously. Keep candidates long enough for map and animgraph startup.
		g_capturePanelUntil.store(GetTickCount64() + 10000, std::memory_order_release);
		original(engine, contextPanel, kCreateScript, "TempleWareModelPreview", 0, true);
		g_status.store(tw::model_preview::Status::AwaitingTexture, std::memory_order_release);
	}

	ID3D11ShaderResourceView* __fastcall hkGetResourceView(void* textureManager, void* binding,
		std::int8_t wantedDimension, bool srgb, const char* parameterName)
	{
		Runtime::CallbackGuard callback;
		const auto result = g_resourceViewHook.GetOriginal()(
			textureManager, binding, wantedDimension, srgb, parameterName);

		if (Runtime::IsShuttingDown() || srgb || result == nullptr)
			return result;

		float textureAspect = 1.0f;
		if (!isPreviewBinding(binding) || !validPreviewTexture(result, &textureAspect))
			return result;

		{
			std::scoped_lock lock(g_textureMutex);
			if (g_texture != result)
			{
				const auto retained = std::find(g_retiredTextures.begin(), g_retiredTextures.end(), result);
				if (retained != g_retiredTextures.end())
					g_retiredTextures.erase(retained);
				else
					result->AddRef();
				if (g_texture != nullptr)
				{
					if (std::find(g_retiredTextures.begin(), g_retiredTextures.end(), g_texture) ==
						g_retiredTextures.end())
					{
						g_retiredTextures.push_back(g_texture);
					}
				}
				g_texture = result;
				g_textureAspect.store(textureAspect, std::memory_order_release);
				g_status.store(tw::model_preview::Status::Ready, std::memory_order_release);
				std::cout << "  model preview: named composition texture captured" << std::endl;
			}
		}
		sampleNativePanel();
		return result;
	}

	void releaseTexture() noexcept
	{
		std::scoped_lock lock(g_textureMutex);
		if (g_texture != nullptr)
			g_texture->Release();
		g_texture = nullptr;
		for (ID3D11ShaderResourceView* texture : g_retiredTextures)
			texture->Release();
		g_retiredTextures.clear();
	}
}

bool tw::model_preview::init() noexcept
{
	{
		std::scoped_lock lock(g_stateMutex);
		g_creationRequested = false;
		g_cleanupRequested = false;
		g_panelSubmitted = false;
		g_lastSubmission = 0;
	}
	g_cleanupComplete.store(true, std::memory_order_release);
	g_status.store(Status::Unavailable, std::memory_order_release);
	g_playerPanel.store(nullptr, std::memory_order_release);
	g_capturePanelUntil.store(0, std::memory_order_release);
	{
		std::scoped_lock lock(g_panelLifetimeMutex);
		g_candidatePanels.fill(nullptr);
		g_candidatePanelCount = 0;
	}
	g_lastBoneSample.store(0, std::memory_order_release);
	g_lastPreviewDraw.store(0, std::memory_order_release);
	g_lastPanelDiagnostic.store(0, std::memory_order_release);
	g_panelSampleFailures.store(0, std::memory_order_release);

	const auto createPlayerPanel = reinterpret_cast<std::uint8_t*>(M::patternScan("client",
		"48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B F1 48 8B FA B9 F0 08 00 00"));
	const auto getBonePanelPosition = reinterpret_cast<void*>(M::patternScan("client",
		"48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? F3 0F 10 0D"));
	if (createPlayerPanel != nullptr && createPlayerPanel[0x42] == 0x48 &&
		createPlayerPanel[0x43] == 0x8D && createPlayerPanel[0x44] == 0x05)
	{
		g_playerPanelVtable = resolveRipTarget(createPlayerPanel + 0x42, 3, 7);
	}
	else
	{
		g_playerPanelVtable = nullptr;
	}
	g_getBonePanelPosition = reinterpret_cast<GetBonePanelPositionFn>(getBonePanelPosition);
	void* destroyPlayerPanel = nullptr;
	if (g_playerPanelVtable != nullptr)
		destroyPlayerPanel = reinterpret_cast<void**>(g_playerPanelVtable)[62];

	const auto runScript = reinterpret_cast<void*>(M::patternScan("panorama",
		"48 89 5C 24 ? 4C 89 4C 24 ? 4C 89 44 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ?"));
	const auto getResourceView = resolveGetResourceView();

	if (runScript == nullptr || getResourceView == nullptr || createPlayerPanel == nullptr ||
		destroyPlayerPanel == nullptr || g_playerPanelVtable == nullptr || g_getBonePanelPosition == nullptr ||
		!g_runScriptHook.Add(runScript, &hkRunScript) ||
		!g_resourceViewHook.Add(getResourceView, &hkGetResourceView) ||
		!g_createPlayerPanelHook.Add(createPlayerPanel, &hkCreatePlayerPanel) ||
		!g_destroyPlayerPanelHook.Add(destroyPlayerPanel, &hkDestroyPlayerPanel) ||
		!g_destroyPlayerPanelHook.Replace() || !g_createPlayerPanelHook.Replace() ||
		!g_resourceViewHook.Replace() || !g_runScriptHook.Replace())
	{
		// A successfully enabled hook remains owned until orderly teardown; removing
		// its trampoline here could race a renderer/Panorama callback.
		if (!g_resourceViewHook.IsHooked())
			g_resourceViewHook.Remove();
		if (!g_runScriptHook.IsHooked())
			g_runScriptHook.Remove();
		if (!g_createPlayerPanelHook.IsHooked())
			g_createPlayerPanelHook.Remove();
		if (!g_destroyPlayerPanelHook.IsHooked())
			g_destroyPlayerPanelHook.Remove();
		// Leave created trampolines owned, but make every partially enabled source inert.
		g_runScriptHook.Restore();
		g_resourceViewHook.Restore();
		g_createPlayerPanelHook.Restore();
		g_destroyPlayerPanelHook.Restore();
		std::cout << "[!] Model preview unavailable - current Panorama/render contracts did not install" << std::endl;
		return false;
	}

	g_status.store(Status::AwaitingPanorama, std::memory_order_release);
	std::cout << "  model preview: Panorama and named texture hooks installed" << std::endl;
	return true;
}

bool tw::model_preview::disableHooks() noexcept
{
	bool success = true;
	{
		// Stop publishing the raw game-owned panel before removing its destructor
		// observer. Taking the same lock as sampling waits for an in-flight helper;
		// later GeneratePrimitives callbacks see null and cannot start another one.
		std::scoped_lock lock(g_panelLifetimeMutex);
		g_playerPanel.store(nullptr, std::memory_order_release);
	}
	success = g_resourceViewHook.Restore() && success;
	success = g_runScriptHook.Restore() && success;
	success = g_createPlayerPanelHook.Restore() && success;
	success = g_destroyPlayerPanelHook.Restore() && success;
	return success;
}

bool tw::model_preview::shutdown() noexcept
{
	bool success = true;
	success = g_resourceViewHook.Remove() && success;
	success = g_runScriptHook.Remove() && success;
	success = g_createPlayerPanelHook.Remove() && success;
	success = g_destroyPlayerPanelHook.Remove() && success;
	g_playerPanel.store(nullptr, std::memory_order_release);
	{
		std::scoped_lock lock(g_panelLifetimeMutex);
		g_candidatePanels.fill(nullptr);
		g_candidatePanelCount = 0;
	}
	{
		std::scoped_lock lock(g_boneMutex);
		g_bones = {};
	}
	releaseTexture();
	g_status.store(Status::Unavailable, std::memory_order_release);
	return success;
}

void tw::model_preview::requestCreation() noexcept
{
	const Status current = g_status.load(std::memory_order_acquire);
	const bool missingNativePanel = g_playerPanel.load(std::memory_order_acquire) == nullptr;
	if (current != Status::AwaitingPanorama && current != Status::AwaitingTexture &&
		!(current == Status::Ready && missingNativePanel))
		return;
	std::scoped_lock lock(g_stateMutex);
	if (current == Status::Ready && missingNativePanel && GetTickCount64() - g_lastSubmission >= 5000)
		g_panelSubmitted = false;
	if (!g_panelSubmitted || GetTickCount64() - g_lastSubmission >= 5000)
		g_creationRequested = true;
}

void tw::model_preview::requestCleanup() noexcept
{
	std::scoped_lock lock(g_stateMutex);
	g_creationRequested = false;
	if (!g_panelSubmitted)
	{
		g_cleanupComplete.store(true, std::memory_order_release);
		return;
	}
	g_cleanupComplete.store(false, std::memory_order_release);
	g_cleanupRequested = true;
}

bool tw::model_preview::cleanupComplete() noexcept
{
	return g_cleanupComplete.load(std::memory_order_acquire);
}

bool tw::model_preview::waitForCleanup(DWORD timeoutMs) noexcept
{
	const ULONGLONG deadline = GetTickCount64() + timeoutMs;
	while (!cleanupComplete() && GetTickCount64() < deadline)
		Sleep(10);
	return cleanupComplete();
}

tw::model_preview::Status tw::model_preview::status() noexcept
{
	return g_status.load(std::memory_order_acquire);
}

const char* tw::model_preview::statusText() noexcept
{
	switch (status())
	{
	case Status::AwaitingPanorama: return "Waiting for an active Panorama context";
	case Status::AwaitingTexture: return "Loading preview composition";
	case Status::Ready: return "Live Panorama preview";
	case Status::Unavailable: return "Preview unavailable for this game build";
	}
	return "Preview unavailable";
}

void tw::model_preview::onGeneratePrimitives() noexcept
{
	sampleNativePanel();
}

bool tw::model_preview::isPreviewSceneObject(void* sceneObject) noexcept
{
	if (sceneObject == nullptr)
		return false;

	void* panel = g_playerPanel.load(std::memory_order_acquire);
	if (panel == nullptr)
		return false;

	std::scoped_lock lifetimeLock(g_panelLifetimeMutex);
	if (g_playerPanel.load(std::memory_order_acquire) != panel)
		return false;
	return sceneObjectMatchesPanelGuarded(panel, sceneObject);
}

void tw::model_preview::draw(const ImVec2& min, const ImVec2& max) noexcept
{
	requestCreation();
	g_lastPreviewDraw.store(GetTickCount64(), std::memory_order_release);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(min, max, IM_COL32(10, 10, 13, 255));

	std::scoped_lock lock(g_textureMutex);
	if (g_texture != nullptr)
	{
		const float width = max.x - min.x;
		const float height = max.y - min.y;

		BoneSnapshot bones{};
		{
			std::scoped_lock boneLock(g_boneMutex);
			bones = g_bones;
		}
		const bool bonesFresh = bones.sampledAt != 0 && GetTickCount64() - bones.sampledAt < 2000;
		const auto toScreen = [&](const float uv[2]) -> ImVec2
		{
			return {
				min.x + width * uv[0],
				min.y + height * uv[1]
			};
		};

		drawList->PushClipRect(min, max, true);
		drawList->AddImage(reinterpret_cast<ImTextureID>(g_texture), min, max);

		if (bonesFresh)
		{
			ImVec2 points[kBoneCount]{};
			float left = max.x;
			float right = min.x;
			float top = max.y;
			float bottom = min.y;
			int visiblePoints = 0;
			for (int index = 0; index < kBoneCount; ++index)
			{
				if (!bones.valid[index])
					continue;
				points[index] = toScreen(bones.uv[index]);
				if (points[index].x < min.x || points[index].x > max.x ||
					points[index].y < min.y || points[index].y > max.y)
				{
					continue;
				}
				left = (std::min)(left, points[index].x);
				right = (std::max)(right, points[index].x);
				top = (std::min)(top, points[index].y);
				bottom = (std::max)(bottom, points[index].y);
				++visiblePoints;
			}

			if (visiblePoints >= 12 && right > left && bottom > top)
			{
				const float bodyHeight = bottom - top;
				left -= bodyHeight * 0.13f;
				right += bodyHeight * 0.13f;
				top -= bodyHeight * 0.04f;
				bottom += bodyHeight * 0.04f;
				const ImVec2 boxMin(left, top);
				const ImVec2 boxMax(right, bottom);

				if (Config::visual_esp.espFill)
					drawList->AddRectFilled(boxMin, boxMax,
						IM_COL32(0, 0, 0, static_cast<int>(Config::visual_esp.espFillOpacity * 170.0f)));
				if (Config::visual_esp.esp)
				{
					const ImU32 color = ImGui::ColorConvertFloat4ToU32(Config::visual_esp.espColor);
					drawList->AddRect(boxMin, boxMax, IM_COL32(0, 0, 0, 220), 0.0f, 0,
						Config::visual_esp.espThickness + 2.0f);
					drawList->AddRect(boxMin, boxMax, color, 0.0f, 0, Config::visual_esp.espThickness);
				}

				if (Config::visual_esp.espSkeleton)
				{
					const ImU32 color = ImGui::ColorConvertFloat4ToU32(Config::visual_esp.skeletonColor);
					for (const auto& connection : kConnections)
					{
						if (!bones.valid[connection[0]] || !bones.valid[connection[1]])
							continue;
						drawList->AddLine(points[connection[0]], points[connection[1]],
							IM_COL32(0, 0, 0, 220), Config::visual_esp.skeletonThickness + 2.0f);
						drawList->AddLine(points[connection[0]], points[connection[1]],
							color, Config::visual_esp.skeletonThickness);
					}
				}

				if (Config::visual_esp.showHealth)
				{
					const float x = boxMin.x - 6.0f;
					drawList->AddRectFilled(ImVec2(x - 2.0f, boxMin.y - 1.0f),
						ImVec2(x + 2.0f, boxMax.y + 1.0f), IM_COL32(0, 0, 0, 220));
					drawList->AddRectFilled(ImVec2(x - 1.0f, boxMin.y),
						ImVec2(x + 1.0f, boxMax.y), IM_COL32(75, 210, 105, 255));
				}

				if (Config::visual_esp.showNameTags)
				{
					constexpr const char* name = "preview";
					const ImVec2 size = ImGui::CalcTextSize(name);
					drawList->AddText(ImVec2((boxMin.x + boxMax.x - size.x) * 0.5f, boxMin.y - size.y - 3.0f),
						IM_COL32(235, 235, 240, 255), name);
				}
				if (Config::visual_esp.showWeapon)
				{
					constexpr const char* weapon = "AWP";
					const ImVec2 size = ImGui::CalcTextSize(weapon);
					drawList->AddText(ImVec2((boxMin.x + boxMax.x - size.x) * 0.5f, boxMax.y + 3.0f),
						IM_COL32(210, 210, 218, 255), weapon);
				}
				if (Config::visual_esp.showScoped)
					drawList->AddText(ImVec2(boxMax.x + 4.0f, boxMin.y), IM_COL32(105, 185, 235, 255), "SCOPED");
			}
		}
		drawList->PopClipRect();
	}
	else
	{
		const char* text = statusText();
		const float wrapWidth = max.x - min.x - 20.0f;
		const ImVec2 size = ImGui::CalcTextSize(text, nullptr, false, wrapWidth);
		drawList->PushClipRect(min, max, true);
		drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
			ImVec2(min.x + (max.x - min.x - size.x) * 0.5f,
				min.y + (max.y - min.y - size.y) * 0.5f),
			IM_COL32(150, 150, 160, 255), text, nullptr, wrapWidth);
		drawList->PopClipRect();
	}
}
