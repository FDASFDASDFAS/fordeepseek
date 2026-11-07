#include "hooks.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>

#include <Windows.h>

#include "../../../external/kiero/minhook/include/MinHook.h"

#include "../../templeware/utils/memory/Interface/Interface.h"
#include "../utils/memory/patternscan/patternscan.h"
#include "../utils/memory/gaa/gaa.h"

#include "../players/hook/playerHook.h"

#include "../features/visuals/visuals.h"
#include "../features/chams/chams.h"
#include "../features/feedback/hit_feedback.h"
#include "../features/tracing/tracing.h"
#include "../features/hitboxes/hitboxes.h"
#include "../features/smokes/smokes.h"
#include "../features/movement/movement.h"
#include "../../velocity/systems/impl/hitbox_discovery.h"
#include "../../velocity/bridge/combat_bridge.h"
#include "../../velocity/bridge/rage_output.h"

#include "../../cs2/datatypes/cutlbuffer/cutlbuffer.h"
#include "../../cs2/datatypes/keyvalues/keyvalues.h"

#include "../../cs2/entity/C_Material/C_Material.h"
#include "../../cs2/entity/C_EntitySystem/entity.h"
#include "../../cs2/entity/C_CSWeaponBase/C_CSWeaponBase.h"

#include "../config/config.h"
#include "../config/configmanager.h"
#include "../commands/command_pipeline.h"
#include "../engine/rtti_registry.h"
#include "../features/core/feature_policies.h"
#include "../interfaces/interfaces.h"
#include "../features/spectatorlist/spectatorlist.h"

namespace
{
	void* pvsState = nullptr;
	tw::commands::CommandRuntimeProbe commandProbe;
	tw::features::TriggerDwell aimReactionDwell;
	tw::features::TriggerDwell triggerDwell;
	tw::features::QuickPeekPolicy quickPeekPolicy;
	CBaseHandle triggerWeaponHandle;
	double triggerReleaseTime{};
	bool quickPeekAttackHeld{};

	bool gameKeyHeld(const int virtualKey) noexcept
	{
		if (virtualKey <= 0 || virtualKey > 0xff)
			return false;
		const HWND foreground = GetForegroundWindow();
		DWORD processId{};
		if (!foreground || GetWindowThreadProcessId(foreground, &processId) == 0 ||
			processId != GetCurrentProcessId())
			return false;
		return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
	}

	struct CommandObservation final
	{
		tw::commands::CommandDraft draft{};
		bool sequencePresent{};
		bool moveCrcPresent{};
		std::uint64_t buttonState2{};
		std::uint64_t buttonState3{};
		bool buttonStatesMatchNative{};
		std::uint32_t historyCount{};
		std::uint32_t historyViewCount{};
		bool firstHistoryMatchesBase{};
		bool lastHistoryMatchesBase{};
		bool historyTruncated{};
		std::uint64_t historyFingerprint{};
	};

	struct LocalMovementObservation final
	{
		tw::entities::EntityKey pawn{};
		tw::entities::Vec3 velocity{};
		std::uint32_t flags{};
		std::int32_t rawMoveType{};
		bool grounded{};
		bool standardWalkType{};
	};

	struct CommandProbeTargets final
	{
		void* createMove{};
		void* moveCrc{};
	};

	void mixHistory(std::uint64_t& hash, const std::uint64_t value) noexcept
	{
		hash ^= value;
		hash *= 1099511628211ULL;
	}

	bool sameAngles(const tw::commands::Angles& left, const CMsgQAngle& right) noexcept
	{
		return left.pitch == right.x() && left.yaw == right.y() && left.roll == right.z();
	}

	void observeHistory(const CUserCmd& command, CommandObservation& observation) noexcept
	{
		constexpr int maximumEntries = 32;
		const auto& user = command.csgoUserCmd;
		const int count = user.input_history_size();
		observation.historyCount = count > 0 ? static_cast<std::uint32_t>(count) : 0U;
		observation.historyTruncated = count > maximumEntries;
		observation.historyFingerprint = 1469598103934665603ULL;
		const int captured = (std::min)(count, maximumEntries);
		for (int index = 0; index < captured; ++index)
		{
			const auto& entry = user.input_history(index);
			mixHistory(observation.historyFingerprint, static_cast<std::uint64_t>(index));
			mixHistory(observation.historyFingerprint, entry.has_view_angles());
			if (entry.has_view_angles())
			{
				const auto& view = entry.view_angles();
				++observation.historyViewCount;
				mixHistory(observation.historyFingerprint, view.has_x());
				mixHistory(observation.historyFingerprint, view.has_y());
				mixHistory(observation.historyFingerprint, view.has_z());
				mixHistory(observation.historyFingerprint, std::bit_cast<std::uint32_t>(view.x()));
				mixHistory(observation.historyFingerprint, std::bit_cast<std::uint32_t>(view.y()));
				mixHistory(observation.historyFingerprint, std::bit_cast<std::uint32_t>(view.z()));
				if (index == 0)
					observation.firstHistoryMatchesBase = sameAngles(observation.draft.view, view);
				if (index == count - 1)
					observation.lastHistoryMatchesBase = sameAngles(observation.draft.view, view);
			}
			mixHistory(observation.historyFingerprint, entry.has_render_tick_count());
			mixHistory(observation.historyFingerprint, static_cast<std::uint32_t>(entry.render_tick_count()));
			mixHistory(observation.historyFingerprint, entry.has_player_tick_count());
			mixHistory(observation.historyFingerprint, static_cast<std::uint32_t>(entry.player_tick_count()));
			mixHistory(observation.historyFingerprint, entry.has_frame_number());
			mixHistory(observation.historyFingerprint, static_cast<std::uint32_t>(entry.frame_number()));
		}
	}

	tw::runtime::Result<CommandObservation> observeCommand(const CUserCmd* command) noexcept
	{
		if (!command || !command->csgoUserCmd.has_base())
			return tw::runtime::Result<CommandObservation>::failure(
				{ tw::runtime::StatusCode::Unavailable, tw::runtime::Subsystem::Commands, 10, 0 });

		const auto& base = command->csgoUserCmd.base();
		if (!base.has_viewangles())
			return tw::runtime::Result<CommandObservation>::failure(
				{ tw::runtime::StatusCode::Unavailable, tw::runtime::Subsystem::Commands, 11, 0 });

		const auto& angles = base.viewangles();
		CommandObservation observation{};
		if (base.has_legacy_command_number() && base.legacy_command_number() >= 0)
		{
			observation.draft.sequence = static_cast<std::uint64_t>(base.legacy_command_number());
			observation.sequencePresent = true;
		}
		else if (base.has_client_tick() && base.client_tick() >= 0)
		{
			// Client tick is used only as temporal probe evidence until command-number ownership is established.
			observation.draft.sequence = static_cast<std::uint64_t>(base.client_tick());
			observation.sequencePresent = true;
		}
		observation.draft.view = { angles.x(), angles.y(), angles.z() };
		observation.draft.movement = { base.forwardmove(), base.leftmove(), base.upmove() };
		if (base.has_buttons_pb())
		{
			const auto& buttons = base.buttons_pb();
			if (buttons.has_buttonstate1())
				observation.draft.buttons = buttons.buttonstate1();
			if (buttons.has_buttonstate2())
				observation.buttonState2 = buttons.buttonstate2();
			if (buttons.has_buttonstate3())
				observation.buttonState3 = buttons.buttonstate3();
		}
		observation.buttonStatesMatchNative =
			observation.draft.buttons == static_cast<std::uint64_t>(command->nButtons.nValue) &&
			observation.buttonState2 == static_cast<std::uint64_t>(command->nButtons.nValueChanged) &&
			observation.buttonState3 == static_cast<std::uint64_t>(command->nButtons.nValueScroll);
		observation.moveCrcPresent = base.has_move_crc();
		observeHistory(*command, observation);
		return tw::runtime::Result<CommandObservation>::success(observation);
	}

	tw::runtime::Result<LocalMovementObservation> observeLocalMovement() noexcept
	{
		if (!I::EngineClient || !I::EntitySystem || !I::GameEntity || !I::GameEntity->Instance ||
			!I::EngineClient->connected() || !I::EngineClient->in_game())
			return tw::runtime::Result<LocalMovementObservation>::failure(
				{ tw::runtime::StatusCode::Unavailable, tw::runtime::Subsystem::Entities, 40, 0 });

		const int localIndex = I::EngineClient->get_local_player();
		if (localIndex <= 0)
			return tw::runtime::Result<LocalMovementObservation>::failure(
				{ tw::runtime::StatusCode::Unavailable, tw::runtime::Subsystem::Entities, 41, 0 });
		auto* controller = I::EntitySystem->get_base_entity<CCSPlayerController>(localIndex);
		if (!controller)
			return tw::runtime::Result<LocalMovementObservation>::failure(
				{ tw::runtime::StatusCode::Unavailable, tw::runtime::Subsystem::Entities, 42, 0 });
		const CBaseHandle pawnHandle = controller->m_hPawn();
		auto* pawn = I::GameEntity->Instance->Get<C_CSPlayerPawn>(pawnHandle);
		const auto key = pawn ? g_entitySystem->key_for(pawn) : std::nullopt;
		if (!pawn || !key || pawn->m_iHealth() <= 0)
			return tw::runtime::Result<LocalMovementObservation>::failure(
				{ tw::runtime::StatusCode::StaleIdentity, tw::runtime::Subsystem::Entities, 43, 0 });

		const Vector_t velocity = pawn->m_vecAbsVelocity();
		if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y) || !std::isfinite(velocity.z))
			return tw::runtime::Result<LocalMovementObservation>::failure(
				{ tw::runtime::StatusCode::RuntimeFailure, tw::runtime::Subsystem::Entities, 44, 0 });
		LocalMovementObservation observation{};
		observation.pawn = *key;
		observation.velocity = { velocity.x, velocity.y, velocity.z };
		observation.flags = pawn->m_fFlags();
		observation.rawMoveType = pawn->m_MoveType();
		observation.grounded = (observation.flags & FL_ONGROUND) != 0;
		observation.standardWalkType = observation.rawMoveType == MOVETYPE_WALK;

		auto* validatedPawn = I::GameEntity->Instance->Get<C_CSPlayerPawn>(pawnHandle);
		const auto validatedKey = validatedPawn ? g_entitySystem->key_for(validatedPawn) : std::nullopt;
		if (validatedPawn != pawn || !validatedKey || *validatedKey != observation.pawn ||
			observation.pawn.epoch != g_entitySystem->session_epoch())
			return tw::runtime::Result<LocalMovementObservation>::failure(
				{ tw::runtime::StatusCode::StaleIdentity, tw::runtime::Subsystem::Entities, 45, 0 });
		return tw::runtime::Result<LocalMovementObservation>::success(observation);
	}

	tw::runtime::Status applyBoundCommand(void* context, const tw::commands::ResolvedCommand& resolved) noexcept
	{
		auto* command = static_cast<CUserCmd*>(context);
		const auto current = observeCommand(command);
		if (!current)
			return tw::runtime::Status::failure(current.error());

		auto* base = command->csgoUserCmd.mutable_base();
		if (!base || !base->has_viewangles())
			return tw::runtime::Status::failure(
				{ tw::runtime::StatusCode::Unavailable, tw::runtime::Subsystem::Commands, 31, 0 });

		// Validate every field before applying the resolved view/movement transaction.
		const auto& next = resolved.command;
		if (next.sequence != current.value().draft.sequence)
			return tw::runtime::Status::failure(
				{ tw::runtime::StatusCode::InvalidArgument, tw::runtime::Subsystem::Commands, 32, 0 });
		// IN_JUMP is unconditionally writable. It used to be gated on
		// probe.jumpTransitionStable, which waits to observe a jump edge in
		// buttons_pb() buttonstate2/3 -- this build reports jump edges through the
		// subtick move steps instead, so that counter never advanced and every
		// bunny-hop commit was rejected here. The standalone build writes jump
		// with no probe at all.
		const std::uint64_t allowedButtonChanges = static_cast<std::uint64_t>(IN_ATTACK) |
			static_cast<std::uint64_t>(IN_SPRINT) |
			static_cast<std::uint64_t>(IN_JUMP);
		// command->nButtons (CInButtonState) is not a safety oracle here: live telemetry shows it
		// disagrees with buttons_pb().buttonstate1() on ~20% of samples even when neither field was
		// touched this boundary (see "Jump transitions: match" in the probe panel). Its leading vtable
		// pointer suggests it is a lazily-refreshed view over the subtick move-step array rather than a
		// value written in lockstep with the finalized command, so it is tracked as diagnostic-only.
		// The buttons_pb() round trip below (read fresh "current", diff against what we want, write only
		// the delta) is self-consistent against a single source of truth and is the real safety net.
		const std::uint64_t changedButtons = next.buttons ^ current.value().draft.buttons;
		if ((changedButtons & ~allowedButtonChanges) != 0)
			return tw::runtime::Status::failure(
				{ tw::runtime::StatusCode::Unsupported, tw::runtime::Subsystem::Commands, 33, 0 });

		if (next.view != current.value().draft.view)
		{
			auto* view = base->mutable_viewangles();
			view->set_x(next.view.pitch);
			view->set_y(next.view.yaw);
			view->set_z(next.view.roll);
			for (int i = 0; i < command->csgoUserCmd.input_history_size(); ++i)
			{
				auto* const entry = command->csgoUserCmd.mutable_input_history(i);
				if (!entry || !entry->has_view_angles())
					continue;
				auto* const historyView = entry->mutable_view_angles();
				historyView->set_x(next.view.pitch);
				historyView->set_y(next.view.yaw);
				historyView->set_z(next.view.roll);
			}
		}
		if (next.movement != current.value().draft.movement)
		{
			const float oldMagnitude = std::hypot(current.value().draft.movement.forward,
				current.value().draft.movement.side);
			const float newMagnitude = std::hypot(next.movement.forward, next.movement.side);
			if (oldMagnitude > 0.001f)
			{
				const float oldDirection = std::atan2(current.value().draft.movement.side,
					current.value().draft.movement.forward);
				const float newDirection = std::atan2(next.movement.side, next.movement.forward);
				const float delta = newDirection - oldDirection;
				const float scale = newMagnitude / oldMagnitude;
				const float cosine = std::cos(delta);
				const float sine = std::sin(delta);
				for (int i = 0; i < base->subtick_moves_size(); ++i)
				{
					auto* const step = base->mutable_subtick_moves(i);
					if (!step)
						continue;
					const float forward = step->has_analog_forward_delta() ? step->analog_forward_delta() : 0.0f;
					const float side = step->has_analog_left_delta() ? step->analog_left_delta() : 0.0f;
					if (step->has_analog_forward_delta())
						step->set_analog_forward_delta((cosine * forward - sine * side) * scale);
					if (step->has_analog_left_delta())
						step->set_analog_left_delta((sine * forward + cosine * side) * scale);
				}
			}
			base->set_forwardmove(next.movement.forward);
			base->set_leftmove(next.movement.side);
			base->set_upmove(next.movement.up);
		}
		// Jump suppression keys on the resolved state rather than on whether
		// buttonstate1 changed. A wheel-bound jump arrives in buttonstate3 (and in
		// the subtick steps) for the command that starts the hop without ever
		// setting buttonstate1, so a state1-only test left scroll jump untouched.
		const std::uint64_t jumpBit = static_cast<std::uint64_t>(IN_JUMP);
		const bool suppressJump = (next.buttons & jumpBit) == 0 &&
			(((current.value().draft.buttons | current.value().buttonState2 |
			   current.value().buttonState3) & jumpBit) != 0);
		if (changedButtons != 0 || suppressJump)
		{
			auto* buttons = base->mutable_buttons_pb();
			buttons->set_buttonstate1(next.buttons);
			command->nButtons.nValue = static_cast<std::int64_t>(next.buttons);
			if ((changedButtons & jumpBit) != 0 || suppressJump)
			{
				if ((next.buttons & static_cast<std::uint64_t>(IN_JUMP)) == 0)
				{
					auto* const steps = base->mutable_subtick_moves();
					for (int i = steps->size() - 1; i >= 0; --i)
					{
						const auto& step = steps->Get(i);
						if (step.has_button() && step.button() == static_cast<std::uint64_t>(IN_JUMP))
							steps->DeleteSubrange(i, 1);
					}
				}
				const std::uint64_t changed = current.value().buttonState2 & ~static_cast<std::uint64_t>(IN_JUMP);
				const std::uint64_t scroll = current.value().buttonState3 & ~static_cast<std::uint64_t>(IN_JUMP);
				buttons->set_buttonstate2(changed);
				buttons->set_buttonstate3(scroll);
				command->nButtons.nValueChanged = static_cast<std::int64_t>(changed);
				command->nButtons.nValueScroll = static_cast<std::int64_t>(scroll);
			}
			if ((changedButtons & static_cast<std::uint64_t>(IN_ATTACK)) != 0)
			{
				// IN_ATTACK must appear in buttonstate2 (changed/pressed-this-frame) for the
				// engine to recognize it as a new shot. buttonstate1 alone (held) is insufficient.
				const std::uint64_t changed = current.value().buttonState2 | static_cast<std::uint64_t>(IN_ATTACK);
				buttons->set_buttonstate2(changed);
				command->nButtons.nValueChanged = static_cast<std::int64_t>(changed);
				if ((next.buttons & static_cast<std::uint64_t>(IN_ATTACK)) != 0 &&
					command->csgoUserCmd.input_history_size() > 0)
				{
					command->csgoUserCmd.set_attack1_start_history_index(
						command->csgoUserCmd.input_history_size() - 1);
				}
			}
		}
		return tw::runtime::Status::success();
	}

	CommandProbeTargets initializeCommandProbe() noexcept
	{
		// Both targets are taken by vtable index, not by hardcoded RVA, and the
		// build fingerprint is recorded as evidence rather than used as a gate.
		// Pinning the timestamp, the image size and three RVAs meant a single
		// game update silently resolved both targets to null -- and because the
		// install sites treat a null target as "nothing to do", neither hook was
		// ever installed and every command-path feature went quiet with no error.
		// What is checked instead is structural and re-verified live: the RTTI
		// vtable this build actually publishes for CCSGOInput, that the live
		// I::Input instance points at that exact vtable, and that it has the
		// expected number of executable entries.
		constexpr std::size_t expectedVtableEntries = 33;
		constexpr std::size_t expectedCreateMoveVtableIndex = 5;
		constexpr std::size_t expectedMoveCrcVtableIndex = 6;
		CommandProbeTargets targets{};

		const HMODULE client = GetModuleHandleA("client.dll");
		if (!client)
			return targets;
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(client);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
			return targets;
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const std::uint8_t*>(client) + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
			return targets;
		commandProbe.setImageIdentity(nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
		const auto rtti = tw::engine::findMsvcVtables(client, ".?AVCCSGOInput@@");
		std::size_t largestVtable{};
		for (const auto& vtable : rtti.vtables)
			largestVtable = (std::max)(largestVtable, vtable.executableEntries);
		const std::uint32_t firstVtableRva = rtti.vtables.empty() ? 0U :
			static_cast<std::uint32_t>(rtti.vtables.front().address - reinterpret_cast<std::uintptr_t>(client));
		commandProbe.setInputRttiEvidence(rtti.typeDescriptorFound, static_cast<std::uint32_t>(rtti.vtables.size()),
			static_cast<std::uint32_t>(largestVtable), firstVtableRva);

		if (!I::Input)
			return targets;

		MEMORY_BASIC_INFORMATION inputMemory{};
		if (VirtualQuery(I::Input, &inputMemory, sizeof(inputMemory)) != sizeof(inputMemory) ||
			(inputMemory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
			return targets;
		const auto liveVtable = *reinterpret_cast<const std::uintptr_t*>(I::Input);

		// The live instance picks the vtable, so this cannot latch onto a
		// same-shaped table that belongs to some other object.
		const auto table = std::find_if(rtti.vtables.begin(), rtti.vtables.end(),
			[liveVtable](const tw::engine::VtableEvidence& value)
		{
			return value.address == liveVtable;
		});
		if (table == rtti.vtables.end() || table->executableEntries != expectedVtableEntries ||
			table->functions.size() <= expectedMoveCrcVtableIndex)
			return targets;

		const auto clientBase = reinterpret_cast<std::uintptr_t>(client);
		const auto createMoveTarget = table->functions[expectedCreateMoveVtableIndex];
		commandProbe.setCommandTargetEvidence(true, static_cast<std::uint32_t>(createMoveTarget - clientBase));
		targets.createMove = reinterpret_cast<void*>(createMoveTarget);

		const auto moveCrcTarget = table->functions[expectedMoveCrcVtableIndex];
		commandProbe.setMoveCrcTargetEvidence(true, static_cast<std::uint32_t>(moveCrcTarget - clientBase));
		targets.moveCrc = reinterpret_cast<void*>(moveCrcTarget);
		return targets;
	}
}

tw::commands::CommandProbeSnapshot H::GetCommandProbeSnapshot() noexcept
{
	return commandProbe.snapshot();
}

void* H::hkOnAddEntity(void* a1, CEntityInstance* entity_instance, int handle)
{
	Runtime::CallbackGuard callback;
	auto original = H::OnAddEntity.GetOriginal();
	if (Runtime::IsShuttingDown())
		return original(a1, entity_instance, handle);

	g_entitySystem->add_entity(entity_instance, handle);

	return original(a1, entity_instance, handle);
}

void* H::hkOnRemoveEntity(void* a1, CEntityInstance* entity_instance, int handle)
{
	Runtime::CallbackGuard callback;
	auto original = H::OnRemoveEntity.GetOriginal();
	if (Runtime::IsShuttingDown())
		return original(a1, entity_instance, handle);

	g_entitySystem->remove_entity(entity_instance, handle);

	return original(a1, entity_instance, handle);
}

void __fastcall H::hkFrameStageNotify(void* a1, int stage)
{
	Runtime::CallbackGuard callback;
	auto original = FrameStageNotify.GetOriginal();
	if (Runtime::IsShuttingDown())
		return original(a1, stage);

	if (!I::EngineClient->connected() || !I::EngineClient->in_game())
	{
		Esp::clear_cache();
		g_spectatorlist->Clear();
		g_ctx->user_cmd = nullptr;
		g_ctx->local_pawn = nullptr;
		g_ctx->local_controller = nullptr;
		return original(a1, stage);
	}

	// Lag-compensation sampling. Belongs here rather than in CreateMove:
	// upstream records at stage 6, once network data has been applied, so the
	// stored origins and bones match what the server last sent.
	velocity::onFrameStage(stage);

	g_ctx->local_pawn = I::EntitySystem->get_local_pawn();
	g_ctx->local_controller = I::EntitySystem->get_base_entity<CCSPlayerController>(I::EngineClient->get_local_player());

	if (g_ctx->local_pawn && g_ctx->local_pawn->m_iHealth() > 0 &&
		internal_config::ConfigManager::Snapshot()->world.antiFlash)
	{
		// Caps the shader-driven whiteout intensity directly; runs every frame rather than through an
		// unverified render-function hook (see hooks.h/antiflash.cpp for why that path was dropped).
		g_ctx->local_pawn->m_flFlashMaxAlpha() = 0.0f;
	}

	if (stage == FRAME_RENDER_END) {
		if (g_ctx->local_pawn)
		{
			Esp::cache();
			g_spectatorlist->Capture();
			chams::cacheChamsTargets();
		}
		else
		{
			Esp::clear_cache();
			g_spectatorlist->Clear();
		}
	}

	original(a1, stage);
}

void* __fastcall H::hkLevelInit(__int64 a1, __int64 a2) {
	Runtime::CallbackGuard callback;
	const auto original = H::LevelInit.GetOriginal();
	if (Runtime::IsShuttingDown())
		return original(a1, a2);

	if (pvsState == nullptr)
	{
		const uintptr_t address = M::patternScan("engine2", "48 8D 0D ? ? ? ? 33 D2 FF 50");
		if (address != 0)
			pvsState = reinterpret_cast<void*>(M::getAbsoluteAddress(address, 0x3));
	}
	if (pvsState != nullptr)
	{
		M::vfunc<void*, 6U, void>(pvsState, false);
	}

	static const auto globalAddr = reinterpret_cast<CGlobalVarsBase**>(M::ResolveRelativeAddress(M::FindPattern("client", ("48 8B 0D ? ? ? ? 4C 8D 05 ? ? ? ? 48 85 D2")), 0x3, 0x7));
	I::GlobalVars = *globalAddr;

	g_entitySystem->level_init();
	commandProbe.resetSession();
	aimReactionDwell.reset();
	triggerDwell.reset();
	quickPeekPolicy.cancel();
	triggerWeaponHandle = {};
	triggerReleaseTime = 0.0;
	quickPeekAttackHeld = false;

	return original(a1, a2);
}

void H::RestoreLevelState()
{
	pvsState = nullptr;
}

void* __fastcall H::hkOnLevelShutdown(void* a1, const char* map_name)
{
	Runtime::CallbackGuard callback;
	auto original = H::LevelShutdown.GetOriginal();
	if (Runtime::IsShuttingDown())
		return original(a1, map_name);

	H::ClearWorldState();
	H::RestoreLevelState();
	g_entitySystem->level_shutdown();
	Esp::clear_cache();
	g_spectatorlist->Clear();
	g_ctx->user_cmd = nullptr;
	g_ctx->local_pawn = nullptr;
	g_ctx->local_controller = nullptr;
	commandProbe.resetSession();
	aimReactionDwell.reset();
	triggerDwell.reset();
	quickPeekPolicy.cancel();
	triggerWeaponHandle = {};
	triggerReleaseTime = 0.0;
	quickPeekAttackHeld = false;

	return original(a1, map_name);
}

void __fastcall H::hkCreateMove(CCSGOInput* rcx, int slot, bool active)
{
	Runtime::CallbackGuard callback;
	auto original = CreateMove.GetOriginal();
	if (Runtime::IsShuttingDown() || !callback.FeatureAdmitted())
		return original(rcx, slot, active);

	CommandObservation beforeObservation{};
	bool beforeAvailable = false;
	CCSPlayerController* localController = g_ctx->local_controller;
	if (rcx && localController)
	{
		const auto before = observeCommand(rcx->get_user_cmd(localController));
		if (before)
		{
			beforeObservation = before.value();
			beforeAvailable = true;
		}
	}

	original(rcx, slot, active);

	localController = g_ctx->local_controller;
	if (!rcx || !localController)
		return;

	// The command comes from the CCSGOInput singleton rather than from the
	// hook's own this pointer, and the controller is resolved off the live pawn,
	// matching the standalone build. get_user_cmd resolves its work through
	// pattern-scanned statics and never touches the instance, so which
	// CCSGOInput it is called on makes no difference to the result -- but going
	// through I::Input keeps this independent of whatever rcx happens to be.
	CCSGOInput* const input = I::Input;
	CUserCmd* userCmd = nullptr;
	if (input)
	{
		C_CSPlayerPawn* const localPawn = g_ctx->local_pawn;
		if (localPawn && localPawn->m_iHealth() > 0)
		{
			CCSPlayerController* const commandController =
				I::GameEntity && I::GameEntity->Instance
					? I::GameEntity->Instance->Get<CCSPlayerController>(localPawn->m_hController())
					: localController;
			userCmd = input->get_user_cmd(commandController ? commandController : localController);
		}
	}

	// Movement runs on the native command here, the way the standalone build
	// does it: after the original CreateMove has filled the command in, and
	// still ahead of the move CRC. It writes nButtons; the bunny-hop intent at
	// the CRC boundary writes buttons_pb(). The two agree on when to strip jump
	// and cover different structures, so they compose rather than fight.
	if (userCmd)
	{
		const auto* const base = userCmd->csgoUserCmd.mutable_base();
		if (base && base->has_viewangles())
		{
			const auto& angles = base->viewangles();
			const Vector_t viewAngle{ angles.x(), angles.y(), angles.z() };
			g_movement->OnCreateMove(userCmd, viewAngle);
		}
	}

	if (!userCmd)
		userCmd = rcx->get_user_cmd(localController);

	const auto observed = observeCommand(userCmd);
	if (!observed)
	{
		commandProbe.recordUnavailable(observed.error().operation);
		return;
	}

	tw::commands::ObservedCommandRuntime commandRuntime{ observed.value().draft };
	tw::commands::UnsupportedPredictionRuntime predictionRuntime;
	tw::commands::CommandPipeline pipeline{ commandRuntime, predictionRuntime };
	const auto passThrough = pipeline.run({});
	commandProbe.record({ GetCurrentThreadId(), observed.value().draft.sequence, observed.value().sequencePresent,
		observed.value().moveCrcPresent, passThrough.ok(), beforeAvailable, beforeObservation.draft.sequence,
		beforeObservation.sequencePresent, beforeObservation.moveCrcPresent });
}

void __fastcall H::hkMoveCrcBoundary(CCSGOInput* rcx, CUserCmd* command)
{
	Runtime::CallbackGuard callback;
	auto original = MoveCrcBoundary.GetOriginal();
	if (Runtime::IsShuttingDown() || !callback.FeatureAdmitted())
		return original(rcx, command);

	const auto before = observeCommand(command);
	const auto movementSnapshot = observeLocalMovement();
	commandProbe.recordMovementSnapshot({ movementSnapshot.ok(),
		movementSnapshot && movementSnapshot.value().standardWalkType,
		movementSnapshot ? movementSnapshot.value().rawMoveType : 0,
		movementSnapshot ? static_cast<std::uint16_t>(0) : movementSnapshot.error().operation });
	bool commitSucceeded = false;
	bool movementMutated = false;
	bool buttonsMutated = false;
	if (before)
	{
		tw::commands::BoundCommandRuntime commandRuntime{ before.value().draft, command, &applyBoundCommand };
		tw::commands::UnsupportedPredictionRuntime predictionRuntime;
		tw::commands::CommandPipeline pipeline{ commandRuntime, predictionRuntime };
		std::vector<tw::commands::CommandIntent> intents;
		const auto probe = commandProbe.snapshot();
		if (probe.moveCrcBoundaryStable)
		{
			const auto settings = internal_config::ConfigManager::Snapshot();
			C_CSPlayerPawn* const localPawn = g_ctx->local_pawn;
			const bool rageActive = settings && settings->rage.enabled;
			const bool aimActivated = settings && settings->aim.enabled &&
				(!settings->aim.onKey || gameKeyHeld(settings->aim.activationKey));
			// Triggerbot disabled. The implementation below stays intact and is
			// still the reference for the crosshair-trace approach, but nothing
			// reaches it: this gate is false regardless of config, and the menu
			// no longer exposes the section (menu.cpp case -2). Flip
			// kTriggerBotEnabled to bring it back.
			static constexpr bool kTriggerBotEnabled = false;
			const bool triggerActivated = kTriggerBotEnabled && settings && settings->trigger.enabled &&
				(!settings->trigger.onKey || gameKeyHeld(settings->trigger.keybind));
			const bool combatRequested = settings &&
				(settings->aim.enabled || (kTriggerBotEnabled && settings->trigger.enabled));
			const float flashDuration = combatRequested && localPawn ? localPawn->m_flFlashDuration() : 0.0f;
			const bool localFlashed = !std::isfinite(flashDuration) || flashDuration > 0.0f;
			const bool localScoped = combatRequested && localPawn && localPawn->m_bIsScoped();
			if (settings && settings->movement.airStrafe && probe.movementSnapshotStable && movementSnapshot &&
				movementSnapshot.value().standardWalkType && !movementSnapshot.value().grounded)
			{
				const auto targetMovement = tw::features::airStrafeMovement(movementSnapshot.value().velocity,
					before.value().draft.view, before.value().draft.movement, before.value().draft.movement.side);
				if (targetMovement)
				{
					tw::commands::CommandIntent strafe{ tw::commands::IntentKind::Movement, 10, 0 };
					strafe.movement = targetMovement.value();
					intents.push_back(strafe);
				}
			}
			// Bunny hop, matched to the standalone build where this works. Two
			// things kept it dead here. The probe gate waited on a jump edge in
			// buttonstate2/3 that this build never reports, and the press test
			// read buttonstate1 only, which misses a wheel-bound jump. Ground
			// state and move type are the only preconditions the working build
			// uses, so they are the only ones left.
			if (settings && settings->movement.bunnyHop && movementSnapshot)
			{
				const std::uint64_t jumpBit = static_cast<std::uint64_t>(IN_JUMP);
				const bool jumpRequested = ((before.value().draft.buttons |
					before.value().buttonState2 | before.value().buttonState3) & jumpBit) != 0;
				const int moveType = static_cast<int>(movementSnapshot.value().rawMoveType);
				const bool restrictedMoveType =
					moveType == MOVETYPE_LADDER || moveType == MOVETYPE_NOCLIP;
				if (jumpRequested && !tw::features::bunnyHopJumpAllowed(jumpRequested,
						movementSnapshot.value().grounded, restrictedMoveType))
				{
					intents.push_back({ tw::commands::IntentKind::Movement, 20, 1, {}, {}, 0, jumpBit });
				}
			}
			if (settings && settings->movement.slowWalk && probe.movementSnapshotStable && movementSnapshot &&
				movementSnapshot.value().standardWalkType && movementSnapshot.value().grounded &&
				std::hypot(before.value().draft.movement.forward, before.value().draft.movement.side) > 0.0f)
			{
				const auto scaled = tw::features::scaleMovementSpeed(before.value().draft.movement, settings->movement.slowWalkScale);
				intents.push_back({ tw::commands::IntentKind::Stance, 20, 2, {}, scaled });
			}
			const bool attackHeld = (before.value().draft.buttons & static_cast<std::uint64_t>(IN_ATTACK)) != 0;
			const bool quickPeekHeld = settings && settings->movement.quickPeek &&
				gameKeyHeld(settings->movement.quickPeekKey);
			if (quickPeekHeld && probe.movementSnapshotStable && movementSnapshot && localPawn)
			{
				const Vector_t origin = localPawn->m_vOldOrigin();
				quickPeekPolicy.activate({ origin.x, origin.y, origin.z });
				if (attackHeld && !quickPeekAttackHeld)
					quickPeekPolicy.onCommittedShot();
				const float inputMagnitude = std::hypot(before.value().draft.movement.forward,
					before.value().draft.movement.side);
				const float returnSpeed = inputMagnitude > 450.0f ? inputMagnitude : 450.0f;
				const auto returnMovement = quickPeekPolicy.update(
					{ origin.x, origin.y, origin.z }, before.value().draft.view.yaw, returnSpeed);
				if (returnMovement)
				{
					tw::commands::CommandIntent peek{ tw::commands::IntentKind::QuickPeekReturn, 25, 3 };
					peek.movement = *returnMovement;
					intents.push_back(peek);
				}
			}
			else
			{
				quickPeekPolicy.cancel();
			}
			quickPeekAttackHeld = attackHeld;
			if (aimActivated && !rageActive && localPawn &&
				(!settings->aim.disableWhenFlashed || !localFlashed) &&
				(!settings->aim.onlyWhileScoped || localScoped))
			{
				const auto aimSnapshot = Esp::snapshotForAim();
				if (aimSnapshot.localValid && aimSnapshot.epoch != 0 && aimSnapshot.epoch == g_entitySystem->session_epoch())
				{
					std::vector<tw::features::AimCandidate> candidates;
					candidates.reserve(aimSnapshot.players.size());
					for (const auto& player : aimSnapshot.players)
					{
						if (player.key.epoch != aimSnapshot.epoch || player.health <= 0)
							continue;

						const Vector_t eye = player.position + player.viewOffset;
						candidates.push_back({ player.key, { eye.x, eye.y, eye.z }, player.team_num });
					}
					const tw::entities::Vec3 localEye{ aimSnapshot.localEyePosition.x, aimSnapshot.localEyePosition.y,
						aimSnapshot.localEyePosition.z };
					const auto target = tw::features::selectAimTarget(localEye, before.value().draft.view, aimSnapshot.localTeam,
						candidates, settings->aim.teamCheck, settings->aim.fov);

					// Same visibility gate the triggerbot uses: never lock onto
					// a target the bullet cannot actually reach.
					bool aimVisible = !settings->aim.visibilityCheck;
					if (target && settings->aim.visibilityCheck && tw::tracing::available())
					{
						std::uintptr_t targetPawn = 0;
						if (I::GameEntity && I::GameEntity->Instance)
							targetPawn = reinterpret_cast<std::uintptr_t>(
								I::GameEntity->Instance->Get<C_CSPlayerPawn>(
									static_cast<int>(target->key.index)));

						if (targetPawn)
							aimVisible = tw::tracing::isVisible(localEye, target->eyePosition, targetPawn,
								reinterpret_cast<std::uintptr_t>(
									static_cast<C_CSPlayerPawn*>(g_ctx->local_pawn)));
					}

					// Smoke is a separate question from the visibility trace: the
					// engine's shot trace passes straight through a cloud, so a
					// target behind smoke reads as perfectly visible. Test the same
					// segment against the smoke volumes and refuse it. Independent
					// of visibilityCheck, since it answers something the trace
					// structurally cannot.
					if (target && aimVisible && settings->aim.smokeCheck &&
						tw::smokes::lineGoesThroughSmoke(localEye, target->eyePosition))
					{
						aimVisible = false;
					}

					bool reactionReady = false;
					if (target && aimVisible)
					{
						const double nowSeconds = I::GlobalVars
							? static_cast<double>(I::GlobalVars->m_current_time) : 0.0;
						reactionReady = aimReactionDwell.update(target->key, nowSeconds,
							static_cast<double>(settings->aim.reactionDelay));
					}
					else
					{
						aimReactionDwell.reset();
					}

					if (target && reactionReady)
					{
						const auto look = tw::features::lookAngles(localEye, target->eyePosition);
						if (look)
						{
							tw::commands::Angles desired = look.value();

							// Aim punch is what the engine adds on top of the command's
							// view angles, both for where the shot goes and for where the
							// crosshair renders. Pinning the view to the target without
							// subtracting it is what walked the crosshair up through a
							// spray: punch kept growing on top of a fixed base angle,
							// and the player's own pull-down was overwritten every
							// command. Subtracting it puts the visible crosshair, and the
							// bullet, back on the target. Scale is 1:1, the same as the
							// ported ragebot uses against the same engine value.
							float punchPitch = 0.0f;
							float punchYaw = 0.0f;
							if (velocity::localAimPunch(localPawn, punchPitch, punchYaw))
							{
								desired.pitch -= punchPitch;
								desired.yaw -= punchYaw;
							}

							const auto smoothed = tw::features::smoothAim(
								before.value().draft.view, desired, settings->aim.smoothing);
							if (smoothed)
								intents.push_back({ tw::commands::IntentKind::Aim, 30, 3, smoothed.value() });
						}
					}
				}
				else
				{
					aimReactionDwell.reset();
				}
			}
			else
			{
				aimReactionDwell.reset();
			}
			if (triggerActivated && !rageActive && localPawn &&
				(!settings->trigger.disableWhenFlashed || !localFlashed) &&
				(!settings->trigger.onlyWhileScoped || localScoped))
			{
				const auto weaponServices = localPawn->GetWeaponServices();
				const CBaseHandle currentWeapon = weaponServices ? weaponServices->m_hActiveWeapon() : CBaseHandle{};
				if (currentWeapon != triggerWeaponHandle)
				{
					triggerDwell.reset();
					triggerReleaseTime = 0.0;
					triggerWeaponHandle = currentWeapon;
				}

				static std::uint32_t triggerDiagFrame{};
				static constexpr std::uint32_t kDiagInterval = 256;
				const bool diag = (triggerDiagFrame++ % kDiagInterval) == 0;
				std::optional<tw::entities::EntityKey> dwellTarget{};
				const auto aimSnapshot = Esp::snapshotForAim();
				if (aimSnapshot.localValid && aimSnapshot.epoch == g_entitySystem->session_epoch())
				{
					std::vector<tw::features::AimCandidate> candidates;
					candidates.reserve(aimSnapshot.players.size());
					for (const auto& player : aimSnapshot.players)
					{
						if (player.key.epoch != aimSnapshot.epoch || player.health <= 0)
							continue;
						if (settings->trigger.teamCheck && player.team_num == aimSnapshot.localTeam)
							continue;
						const Vector_t eye = player.position + player.viewOffset;
						candidates.push_back({ player.key, { eye.x, eye.y, eye.z }, player.team_num });
					}
					if (diag && !candidates.empty())
						std::cout << "[trigger] candidates=" << candidates.size() << " localValid=" << aimSnapshot.localValid
							<< " localTeam=" << aimSnapshot.localTeam << " epoch=" << aimSnapshot.epoch << std::endl;
					const tw::entities::Vec3 localEye{ aimSnapshot.localEyePosition.x,
						aimSnapshot.localEyePosition.y, aimSnapshot.localEyePosition.z };
					// A FOV cone is the wrong test for a triggerbot: at 5 degrees it
					// accepts anything vaguely near the crosshair, which is why it
					// fired at empty space. Trace straight down the view ray and
					// only accept when an enemy pawn is the first thing hit.
					std::optional<tw::features::AimCandidate> target{};

					if (tw::tracing::available())
					{
						Vector_t forward{};
						const Vector_t viewAngles{
							before.value().draft.view.pitch,
							before.value().draft.view.yaw,
							0.0f };
						AngleVectors(viewAngles, &forward);

						const tw::entities::Vec3 rayEnd{
							localEye.x + forward.x * 8192.0f,
							localEye.y + forward.y * 8192.0f,
							localEye.z + forward.z * 8192.0f };

						const auto shot = tw::tracing::trace(localEye, rayEnd,
							reinterpret_cast<std::uintptr_t>(
								static_cast<C_CSPlayerPawn*>(g_ctx->local_pawn)));

						// The trace reports the CHitBox it struck, so the body part
						// is the game's own answer rather than something inferred
						// from bone positions.
						const int hitGroup = tw::hitboxes::groupOf(shot.hitboxData);

						// RE probe for the hitbox/bone chain. Disabled by default
						// (systems::hitbox_discovery::g_enabled) now that the
						// chain is baked into systems::hitboxes::query -- flip it
						// on after a game update to re-verify the offsets.
						//
						// This is the only place the game hands us a real CHitBox
						// pointer, which is the ground truth the search anchors
						// on, so the call site stays even though it is dormant.
						if (systems::hitbox_discovery::g_enabled && shot.hitboxData && shot.hitEntity)
						{
							auto* const hitPawn = reinterpret_cast<C_CSPlayerPawn*>(shot.hitEntity);
							const auto* const sceneNode = hitPawn->m_pGameSceneNode();

							if (sceneNode)
							{
								systems::hitbox_discovery::report(
									shot.hitboxData,
									reinterpret_cast<std::uintptr_t>(sceneNode));
							}
							else if (diag)
							{
								// Say so rather than skipping quietly -- a silent
								// skip here is indistinguishable from the probe
								// never running.
								std::cout << "[hitbox-re] no scene node for hit entity "
									<< reinterpret_cast<void*>(shot.hitEntity) << std::endl;
							}
						}

						if (shot.valid && shot.hitEntity && I::GameEntity && I::GameEntity->Instance)
						{
							for (const auto& candidate : candidates)
							{
								const auto pawn = reinterpret_cast<std::uintptr_t>(
									I::GameEntity->Instance->Get<C_CSPlayerPawn>(
										static_cast<int>(candidate.key.index)));
								if (pawn && pawn == shot.hitEntity)
								{
									target = candidate;
									break;
								}
							}
						}

						// Head only: refuse anything the game does not classify as
						// the head. An unreadable hitbox also refuses, so the
						// option never degrades into "shoot everything".
						if (target && settings->trigger.headOnly && hitGroup != tw::hitboxes::kHead)
							target.reset();

						if (diag)
							std::cout << "[trigger] crosshair hit=" << reinterpret_cast<void*>(shot.hitEntity)
								<< " enemy=" << (target.has_value() ? "YES" : "no")
								<< " hitbox=" << reinterpret_cast<void*>(shot.hitboxData)
								<< " group=" << hitGroup << " (" << tw::hitboxes::groupName(hitGroup) << ")"
								<< " fraction=" << shot.fraction << std::endl;
					}
					else if (diag)
						std::cout << "[trigger] tracing unavailable; refusing fallback fire" << std::endl;
					if (target)
					{
						// Already proven visible: the crosshair ray hit this pawn
						// as the first thing in its path.
						dwellTarget = target->key;
					}
				}
				else if (diag)
					std::cout << "[trigger] snapshot invalid localValid=" << aimSnapshot.localValid
						<< " snapshotEpoch=" << aimSnapshot.epoch
						<< " sessionEpoch=" << g_entitySystem->session_epoch() << std::endl;
				if (!dwellTarget)
				{
					triggerDwell.reset();
					triggerReleaseTime = 0.0;
				}
				else
				{
					bool weaponReady = false;
					if (localPawn && I::GlobalVars)
					{
						const auto weaponServices = localPawn->GetWeaponServices();
						const auto activeWeapon = localPawn->GetActiveWeapon();
						if (weaponServices && activeWeapon)
						{
							const bool hasAmmo = activeWeapon->clip1() > 0;
							const bool notReloading = !activeWeapon->m_bInReload();
							const bool playerReady = weaponServices->m_flNextAttack() <= I::GlobalVars->m_current_time;
							const bool weaponCycleReady = activeWeapon->m_nNextPrimaryAttackTick() <= I::GlobalVars->m_tick_count;
							weaponReady = hasAmmo && notReloading && playerReady && weaponCycleReady;
						}
					}
					if (!weaponReady)
					{
						// Weapon cycling — skip the dwell update this frame but DON'T reset
						// the timer. When the weapon is ready again, the dwell continues.
						if (diag)
							std::cout << "[trigger] weapon not ready, dwell waiting" << std::endl;
					}
					else
					{
						const double nowSeconds = I::GlobalVars ? static_cast<double>(I::GlobalVars->m_current_time) : 0.0;

						// velocity-style release latch. Without this the dwell keeps
						// reporting "fired" every tick while the target stays valid,
						// so IN_ATTACK was held down continuously instead of pulsing
						// — that is the [trigger] FIRED spam.
						const double kAttackHoldSeconds = static_cast<double>(settings->trigger.attackHold);
						auto emitTriggerFire = [&intents, &before]
						{
							tw::commands::CommandIntent fire{};
							fire.kind = tw::commands::IntentKind::Fire;
							fire.priority = 35;
							fire.stableOrdinal = 5;
							// The trace validated this exact view. Keep Trigger's shot atomic so a
							// lower-priority Legit aim cannot redirect it after target confirmation.
							fire.view = before.value().draft.view;
							fire.setButtons = static_cast<std::uint64_t>(IN_ATTACK);
							fire.requestsShot = true;
							intents.push_back(fire);
						};
						bool holdingAttack = false;

						if (triggerReleaseTime > 0.0)
						{
							if (nowSeconds >= triggerReleaseTime)
							{
								// Release and require the target to be re-acquired.
								triggerReleaseTime = 0.0;
								dwellTarget.reset();
							}
							else
							{
								emitTriggerFire();
								holdingAttack = true;
							}
						}

						if (!holdingAttack)
						{
							const bool fired = triggerDwell.update(dwellTarget, nowSeconds,
								static_cast<double>(settings->trigger.dwellDelay));
							if (fired)
							{
								triggerReleaseTime = nowSeconds + kAttackHoldSeconds;
								triggerDwell.reset();
								emitTriggerFire();
								if (diag)
									std::cout << "[trigger] FIRED" << std::endl;
							}
							else if (diag)
								std::cout << "[trigger] dwell now=" << nowSeconds << " delay=" << settings->trigger.dwellDelay << std::endl;
						}
					}
				}
			}
			else
			{
				triggerDwell.reset();
				triggerWeaponHandle = {};
				triggerReleaseTime = 0.0;
			}

			const bool localAttacking = ((before.value().draft.buttons & static_cast<std::uint64_t>(IN_ATTACK)) != 0);
			static bool wasAttackingLastFrame = false;
			if (localAttacking && !wasAttackingLastFrame && g_ctx->local_pawn)
			{
				const float pitchRad = before.value().draft.view.pitch * 0.017453292f;
				const float yawRad = before.value().draft.view.yaw * 0.017453292f;
				const float cosPitch = std::cos(pitchRad);
				const tw::entities::Vec3 forward{
					cosPitch * std::cos(yawRad),
					cosPitch * std::sin(yawRad),
					-std::sin(pitchRad)
				};
				const Vector_t eye = g_ctx->local_pawn->m_vOldOrigin() + g_ctx->local_pawn->m_vecViewOffset();
				const double nowSec = I::GlobalVars ? static_cast<double>(I::GlobalVars->m_current_time) : 0.0;
				tw::features::g_hitFeedback.onShotFired({ eye.x, eye.y, eye.z }, forward, nowSec);
			}
			wasAttackingLastFrame = localAttacking;
		}
		// Ported ragebot output.
		//
		// rage::fire_gun publishes its chosen angle and fire decision rather than
		// writing the usercmd itself (see velocity/bridge/rage_output.h). It is
		// delivered here because this boundary runs BEFORE the game computes the
		// move CRC -- so the CRC covers our angle automatically and velocity's
		// serialize_move_crc call, which was never verified on this build, is not
		// needed at all.
		//
		// Priority 40 puts it above the existing aim intent (30) and the movement
		// ones (10/20); the resolver handles the precedence.
		// Produce and consume Rage at the same pre-CRC boundary. Producing it after
		// original CreateMove returned made every decision one command late.
		if (probe.moveCrcBoundaryStable)
			velocity::onCreateMove();
		const auto rage = velocity::consumeRageDecision();
		const auto rageSettings = internal_config::ConfigManager::Snapshot();
		if (probe.moveCrcBoundaryStable && rageSettings && rageSettings->rage.enabled
			&& rage.valid && before.value().sequencePresent
			&& rage.sequence == before.value().draft.sequence)
		{
			tw::commands::CommandIntent intent{};
			intent.kind = tw::commands::IntentKind::Aim;
			intent.priority = 40;
			intent.stableOrdinal = 4;
			intent.view = tw::commands::Angles{ rage.pitch, rage.yaw, rage.roll };

			if (rage.wantsAttack)
			{
				intent.setButtons = static_cast<std::uint64_t>(IN_ATTACK);
				intent.requestsShot = true;
			}

			intents.push_back(intent);

			static bool loggedOnce = false;
			if (!loggedOnce)
			{
				loggedOnce = true;
				std::cout << "[combat] ragebot intent published at move-CRC boundary"
					<< " (pitch=" << rage.pitch << " yaw=" << rage.yaw
					<< " attack=" << rage.wantsAttack << ")" << std::endl;
			}
		}
		else if (probe.moveCrcBoundaryStable && rageSettings && rageSettings->rage.enabled && rage.valid)
		{
			static bool loggedMismatch = false;
			if (!loggedMismatch)
			{
				loggedMismatch = true;
				std::cout << "[combat] dropped rage decision: decisionSequence=" << rage.sequence
					<< " boundarySequence=" << before.value().draft.sequence
					<< " boundarySequencePresent=" << before.value().sequencePresent << std::endl;
			}
		}

		const auto committed = pipeline.run(std::move(intents));
		commitSucceeded = committed.ok();
		if (committed && committed.value().shotCommitted)
			quickPeekPolicy.onCommittedShot();
		movementMutated = committed && committed.value().command.movement != before.value().draft.movement;
		buttonsMutated = committed && committed.value().command.buttons != before.value().draft.buttons;
	}
	original(rcx, command);
	const auto after = observeCommand(command);

	std::uint16_t unavailableOperation{};
	if (!before)
		unavailableOperation = before.error().operation;
	else if (!after)
		unavailableOperation = after.error().operation;
	commandProbe.recordMoveCrcBoundary({ GetCurrentThreadId(), before.ok(), before && before.value().moveCrcPresent,
		after.ok(), after && after.value().moveCrcPresent, commitSucceeded, movementMutated, buttonsMutated,
		unavailableOperation });
	if (before)
	{
		const bool jumpPressed = (before.value().draft.buttons & static_cast<std::uint64_t>(IN_JUMP)) != 0;
		const bool jumpTransition = ((before.value().buttonState2 | before.value().buttonState3) &
			static_cast<std::uint64_t>(IN_JUMP)) != 0;
		commandProbe.recordButtonTransition({ before.value().buttonStatesMatchNative, jumpPressed, jumpTransition });
	}
	if (before && after)
	{
		const bool unchanged = before.value().historyCount == after.value().historyCount &&
			before.value().historyFingerprint == after.value().historyFingerprint;
		commandProbe.recordViewHistory({ before.value().historyCount, before.value().historyViewCount,
			before.value().firstHistoryMatchesBase, before.value().lastHistoryMatchesBase, unchanged,
			before.value().historyTruncated || after.value().historyTruncated });
	}
}

bool H::Hooks::init() {
	const auto commandTargets = initializeCommandProbe();
	const uintptr_t weaponData = M::patternScan("client", "48 8B 81 ? ? ? ? 85 D2 78 ? 48 83 FA ? 73 ? F3 0F 10 84 90 ? ? ? ? C3 F3 0F 10 80 ? ? ? ? C3 CC CC CC CC");
	const uintptr_t baseEntity = M::patternScan("client", "4C 8D 49 10 81 FA FE 7F 00 00 ? ? 8B CA C1 F9 09 83 F9 3F ? ? 48 63 C1 4D");
	if (weaponData == 0 || baseEntity == 0 || I::Input == nullptr)
		return false;

	oGetWeaponData = *reinterpret_cast<int*>(weaponData + 0x3);
	ogGetBaseEntity = reinterpret_cast<decltype(ogGetBaseEntity)>(baseEntity);

	const auto frameStageNotify = reinterpret_cast<void*>(M::patternScan("client", "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED"));
	const auto levelInit = reinterpret_cast<void*>(M::patternScan("client", "48 89 74 24 ? 57 48 83 EC ? 48 8B 0D ? ? ? ? 48 8B FA"));
	const auto levelShutdown = reinterpret_cast<void*>(M::patternScan("client", "48 83 EC ? 48 8B 0D ? ? ? ? 48 8D 15 ? ? ? ? 45 33 C9 45 33 C0 48 8B 01 FF 50 ? 48 85 C0 74 ? 48 8B 0D ? ? ? ? 48 8B D0 4C 8B 01 41 FF 50 ? 48 83 C4"));
	const auto onAddEntity = reinterpret_cast<void*>(M::patternScan("client", "48 89 74 24 ? 57 48 83 EC ? 41 B9 ? ? ? ? 41 8B C0 41 23 C1 48 8B F2 41 83 F8 ? 48 8B F9 44 0F 45 C8 41 81 F9 ? ? ? ? 73 ? FF 81"));
	const auto onRemoveEntity = reinterpret_cast<void*>(M::patternScan("client", "48 89 74 24 ? 57 48 83 EC ? 41 B9 ? ? ? ? 41 8B C0 41 23 C1 48 8B F2 41 83 F8 ? 48 8B F9 44 0F 45 C8 41 81 F9 ? ? ? ? 73 ? FF 89"));
	// Command hooks remain observational even after their exact current-image targets validate.

	// World/visuals hooks (custom FOV, night mode): optional, like the command hooks above. Verified
	// byte patterns (cs2-universal-offsets, build 14172); menu controls only unhide once IsHooked()
	// actually confirms the install succeeded on this running image. Anti-flash is NOT here: the
	// "FlashOverlay" pattern this dump offered targets an unidentified, confirmed-wrong function --
	// anti-flash runs as a schema write in hkFrameStageNotify instead (see below).
	const auto worldFovResolver = reinterpret_cast<void*>(M::patternScan("client", "40 53 48 83 EC 50 48 8B D9 E8 ? ? ? ? 48 85 C0 74 ? 48 8B C8 48 83 C4 50 5B E9"));
	const auto updateLightObject = reinterpret_cast<void*>(M::patternScan("scenesystem", "48 89 54 24 ? 55 57 41 56 48 83 EC"));
	const auto drawAggregateSceneObjectArray = reinterpret_cast<void*>(M::patternScan("scenesystem", "48 8B C4 48 89 50 ? 48 89 48 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70"));
	// generatePrimitives in scenesystem.dll — verified from cs2-universal-offsets, build 14172.
	const auto generatePrimitives = reinterpret_cast<void*>(M::patternScan("scenesystem",
		"48 8B C4 48 89 58 ? 48 89 50 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70 ? 45 33 F6"));

	if (frameStageNotify == nullptr || levelInit == nullptr || levelShutdown == nullptr ||
		onAddEntity == nullptr || onRemoveEntity == nullptr)
		return false;

	// A null command target used to be indistinguishable from a healthy install:
	// every guard below reads it as "nothing to hook" and reports success, so the
	// whole command path could go missing without a single line of output.
	if (commandTargets.createMove == nullptr || commandTargets.moveCrc == nullptr)
		std::cout << "[!] CCSGOInput command targets unresolved (createMove="
			<< (commandTargets.createMove ? "ok" : "null") << ", moveCrc="
			<< (commandTargets.moveCrc ? "ok" : "null")
			<< ") - aim, movement and smoke checks cannot run this session" << std::endl;
	else
		std::cout << "  command hooks: createMove + moveCrc resolved" << std::endl;

	const bool created =
		FrameStageNotify.Add(frameStageNotify, &hkFrameStageNotify) &&
		LevelInit.Add(levelInit, &hkLevelInit) &&
		LevelShutdown.Add(levelShutdown, &hkOnLevelShutdown) &&
		OnAddEntity.Add(onAddEntity, &hkOnAddEntity) &&
		OnRemoveEntity.Add(onRemoveEntity, &hkOnRemoveEntity) &&
		(commandTargets.moveCrc == nullptr || MoveCrcBoundary.Add(commandTargets.moveCrc, &hkMoveCrcBoundary)) &&
		(commandTargets.createMove == nullptr || CreateMove.Add(commandTargets.createMove, &hkCreateMove)) &&
		(worldFovResolver == nullptr || GetRenderFov.Add(worldFovResolver, &hkGetRenderFov)) &&
		(updateLightObject == nullptr || UpdateLightObject.Add(updateLightObject, &hkUpdateLightObject)) &&
		(drawAggregateSceneObjectArray == nullptr ||
			UpdateAggregateSceneObject.Add(drawAggregateSceneObjectArray, &hkUpdateAggregateSceneObject)) &&
		(generatePrimitives == nullptr || GeneratePrimitives.Add(generatePrimitives, chams::hkGeneratePrimitives));

	if (generatePrimitives == nullptr)
		std::cout << "[!] GeneratePrimitives pattern not found — chams disabled this session" << std::endl;
	else if (!created)
		std::cout << "[!] Hook creation failed" << std::endl;

	const bool enabled = created &&
		FrameStageNotify.Replace() &&
		LevelInit.Replace() &&
		LevelShutdown.Replace() &&
		OnAddEntity.Replace() &&
		OnRemoveEntity.Replace() &&
		(commandTargets.moveCrc == nullptr || MoveCrcBoundary.Replace()) &&
		(commandTargets.createMove == nullptr || CreateMove.Replace()) &&
		(worldFovResolver == nullptr || GetRenderFov.Replace()) &&
		(updateLightObject == nullptr || UpdateLightObject.Replace()) &&
		(drawAggregateSceneObjectArray == nullptr || UpdateAggregateSceneObject.Replace()) &&
		(generatePrimitives == nullptr || GeneratePrimitives.Replace());

	if (!enabled)
	{
		shutdown();
		return false;
	}
	return true;
}

bool H::Hooks::shutdown() {
	bool success = true;
	success = GeneratePrimitives.Remove() && success;
	success = UpdateLightObject.Remove() && success;
	success = UpdateAggregateSceneObject.Remove() && success;
	success = OnRemoveEntity.Remove() && success;
	success = OnAddEntity.Remove() && success;
	success = RenderFlashBangOverlay.Remove() && success;
	success = LevelShutdown.Remove() && success;
	success = LevelInit.Remove() && success;
	success = GetRenderFov.Remove() && success;
	success = DrawArray.Remove() && success;
	success = FrameStageNotify.Remove() && success;
	success = CreateMove.Remove() && success;
	success = MoveCrcBoundary.Remove() && success;
	return success;
}

bool H::Hooks::disable() {
	bool success = true;
	success = GeneratePrimitives.Restore() && success;
	success = UpdateLightObject.Restore() && success;
	success = UpdateAggregateSceneObject.Restore() && success;
	success = OnRemoveEntity.Restore() && success;
	success = OnAddEntity.Restore() && success;
	success = RenderFlashBangOverlay.Restore() && success;
	success = LevelShutdown.Restore() && success;
	success = LevelInit.Restore() && success;
	success = GetRenderFov.Restore() && success;
	success = DrawArray.Restore() && success;
	success = FrameStageNotify.Restore() && success;
	success = CreateMove.Restore() && success;
	success = MoveCrcBoundary.Restore() && success;
	return success;
}
