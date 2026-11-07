#pragma once
#include "../../../external/imgui/imgui.h"

#include <array>

namespace Config {

	struct visual_esp_t {
		bool esp = false;
		bool showHealth = false;
		bool teamCheck = false;
		bool espFill = false;
		float espThickness = 1.0f;
		float espFillOpacity = 0.5f;
		ImVec4 espColor = ImVec4(1, 0, 0, 1);
		bool showNameTags = false;
		bool showFlashed = false;
		bool showScoped = false;

		bool espSkeleton = false;
		ImVec4 skeletonColor = ImVec4(1, 0, 0, 1);
		float skeletonThickness = 1.0f;
		bool showWeapon = false;
		bool showItems = false;
		bool showGrenades = false;
		bool showBomb = false;
	};
	extern visual_esp_t visual_esp;

	struct visual_world_t {
		bool Night = false;
		ImVec4 NightColor = ImVec4(0.1, 0.1, 0.1, 1);

		bool fovEnabled = false;
		float fov = 90.0f;

		bool spectatorList = false;

		bool antiflash = false;
		bool crosshair = false;
		bool velocityCounter = false;
	};
	extern visual_world_t visual_world;

	struct visuals_chams_t {
		bool enemyChamsInvisible = false;
		bool enemyChams = false;
		bool teamChams = false;
		bool teamChamsInvisible = false;
		bool localChams = false;
		int chamsMaterial = 0;


		bool armChams = false;
		bool viewmodelChams = false;
		ImVec4 colViewmodelChams = ImVec4(1, 0, 0, 1);
		ImVec4 colArmChams = ImVec4(1, 0, 0, 1);

		ImVec4 colVisualChams = ImVec4(1, 0, 0, 1);
		ImVec4 colVisualChamsIgnoreZ = ImVec4(1, 0, 0, 1);
		ImVec4 teamcolVisualChamsIgnoreZ = ImVec4(1, 0, 0, 1);
		ImVec4 teamcolVisualChams = ImVec4(1, 0, 0, 1);
		ImVec4 localcolChams = ImVec4(1, 0, 0, 1);
	};
	extern visuals_chams_t visual_chams;

	struct rage_weapon_t {
		bool bodyAim = false;
		bool forceShotAir = false;
		bool forceShot = false;
		float maxFov = 180.0f;
		int hitchance = 80;
		int minDamage = 1;
		bool minDamageOverride = false;
		int minDamageOverrideValue = 11;
		bool hitchanceOverride = false;
		int hitchanceOverrideValue = 75;
		float pointScale = 85.0f;
		bool dynamicPointScale = true;
		int hitboxes = 0x3f;
	};

	struct rage_t {
		bool enabled = false;
		int selectedWeaponGroup = 0;
		int maxBacktrackTicks = 12;
		bool extrapolation = true;
		int maxExtrapolateTicks = 8;
		std::array<rage_weapon_t, 6> weaponGroups{};
	};
	extern rage_t rage;

	struct legit_aim_t {
		bool aimbot = 0;
		float aimbot_fov = 0;
		bool team_check = true;
		bool rcs = 0;
		bool fov_circle = 0;
		ImVec4 fovCircleColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		float smoothing = 1.0f;
		float reactionDelay = 0.0f;
		bool targetHeadBone = false;
		bool visibilityCheck = true;
		bool disableWhenFlashed = true;
		bool onlyWhileScoped = false;
		int activationKey = 0x06;
		bool onKey = true;
		bool smokeCheck = true;
	};
	extern legit_aim_t legit_aim;

	struct anti_aim_t
	{
		bool enabled;
		bool overridePitch;
		bool overrideYaw;
		int pitchAmount = 0;
		int yawAmount = 0;
		int jitterAmount = 0;

	};
	extern anti_aim_t anti_aim;

	struct trigger_t {
		bool enabled = false;
		float dwellDelay = 0.05f;
		bool headOnly = false;
		bool teamCheck = true;
		float fov = 5.0f;
		int keybind = 0x05;
		bool onKey = true;
		float attackHold = 0.03f;
		bool disableWhenFlashed = true;
		bool onlyWhileScoped = false;
	};
	extern trigger_t trigger;

	struct feedback_t {
		bool hitMarker = true;
		bool hitSound = true;
		int soundType = 0;
		float soundVolume = 0.8f;
		bool hitLogs = true;
		bool bulletTracers = true;
		bool bulletImpacts = true;
		ImVec4 tracerColor = ImVec4(0.2f, 0.8f, 1.0f, 0.8f);
		ImVec4 impactColor = ImVec4(1.0f, 0.3f, 0.3f, 0.8f);
		bool separateHeadshotSound = false;
		int headshotSoundType = 0;
	};
	extern feedback_t feedback;

	struct watermark_t {
		bool enabled = true;
		// 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right.
		int position = 0;
		bool showFps = true;
		bool showTime = true;
		bool showVersion = true;
		// Off lets the watermark keep its own colour when the menu accent moves.
		bool followAccent = true;
		ImVec4 color = ImVec4(0.263f, 0.717f, 0.898f, 1.0f);
	};
	extern watermark_t watermark;

	struct movement_t {
		bool bhop = false;
		bool airStrafe = false;
		bool slowWalk = false;
		float slowWalkScale = 0.50f;
		bool quickPeek = false;
		int quickPeekKey = 'V';
	};
	extern movement_t movement;
}
