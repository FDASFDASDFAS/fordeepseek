#include "configmanager.h"

#include "config.h"
#include "settings_store.h"
#include "../menu/gui/styling.h"

#include <Windows.h>

#include <cstdlib>
#include <filesystem>

namespace internal_config
{
    namespace
    {
        constexpr const char* kConfigFolder = ".dorkcat";
        constexpr const char* kLegacyConfigFolder = ".templeware";

        std::filesystem::path userRoot()
        {
            char* profile{};
            std::size_t length{};
            const errno_t result = _dupenv_s(&profile, &length, "USERPROFILE");
            std::filesystem::path root = result == 0 && profile != nullptr && length != 0
                ? std::filesystem::path{ profile } : std::filesystem::path{};
            free(profile);
            return root;
        }

        std::filesystem::path folderFor(const char* name)
        {
            const std::filesystem::path root = userRoot();
            std::filesystem::path directory = root.empty() ? std::filesystem::path{ name } : root;
            if (directory.filename() != name)
                directory /= name;
            return directory / "internal";
        }

        // Copies configs written under the old folder name across the first time
        // the new one is used. Copy rather than move, and never overwrite: if the
        // user runs an older build again it still finds its configs, and a partial
        // copy cannot destroy the originals.
        void migrateLegacyConfigs(const std::filesystem::path& target)
        {
            std::error_code error;
            if (std::filesystem::exists(target, error))
                return;
            const auto legacy = folderFor(kLegacyConfigFolder);
            if (!std::filesystem::exists(legacy, error) || error)
                return;
            std::filesystem::create_directories(target, error);
            if (error)
                return;
            std::filesystem::copy(legacy, target,
                std::filesystem::copy_options::skip_existing, error);
        }

        std::filesystem::path configDirectory()
        {
            const auto directory = folderFor(kConfigFolder);
            migrateLegacyConfigs(directory);
            return directory;
        }

        tw::config::Color4 copyColor(const ImVec4& value) noexcept { return { value.x, value.y, value.z, value.w }; }
        ImVec4 copyColor(const tw::config::Color4& value) noexcept { return { value.r, value.g, value.b, value.a }; }

        tw::config::Settings captureLegacy()
        {
            tw::config::Settings value{};
            value.esp = { Config::visual_esp.esp, Config::visual_esp.showHealth, Config::visual_esp.teamCheck,
                Config::visual_esp.espFill, Config::visual_esp.espThickness, Config::visual_esp.espFillOpacity,
                copyColor(Config::visual_esp.espColor), Config::visual_esp.showNameTags, Config::visual_esp.showFlashed,
                Config::visual_esp.showScoped, Config::visual_esp.espSkeleton, copyColor(Config::visual_esp.skeletonColor),
                Config::visual_esp.skeletonThickness, Config::visual_esp.showWeapon, Config::visual_esp.showItems,
                Config::visual_esp.showGrenades, Config::visual_esp.showBomb };
			value.world = { Config::visual_world.Night, copyColor(Config::visual_world.NightColor), Config::visual_world.fovEnabled,
				Config::visual_world.fov, Config::visual_world.spectatorList, Config::visual_world.antiflash,
				Config::visual_world.crosshair, Config::visual_world.velocityCounter };
			value.chams = { Config::visual_chams.enemyChams, Config::visual_chams.enemyChamsInvisible,
                Config::visual_chams.teamChams, Config::visual_chams.teamChamsInvisible, Config::visual_chams.localChams,
                static_cast<std::uint8_t>(Config::visual_chams.chamsMaterial), Config::visual_chams.armChams,
                Config::visual_chams.viewmodelChams, copyColor(Config::visual_chams.colVisualChams),
                copyColor(Config::visual_chams.colVisualChamsIgnoreZ), copyColor(Config::visual_chams.teamcolVisualChams),
				copyColor(Config::visual_chams.teamcolVisualChamsIgnoreZ), copyColor(Config::visual_chams.localcolChams),
				copyColor(Config::visual_chams.colArmChams), copyColor(Config::visual_chams.colViewmodelChams) };
			value.rage.enabled = Config::rage.enabled;
			value.rage.selectedWeaponGroup = Config::rage.selectedWeaponGroup;
			value.rage.maxBacktrackTicks = Config::rage.maxBacktrackTicks;
			value.rage.extrapolation = Config::rage.extrapolation;
			value.rage.maxExtrapolateTicks = Config::rage.maxExtrapolateTicks;
			for (std::size_t i = 0; i < value.rage.weaponGroups.size(); ++i)
			{
				const auto& source = Config::rage.weaponGroups[i];
				auto& target = value.rage.weaponGroups[i];
				target = { source.bodyAim, source.forceShotAir, source.forceShot, source.maxFov,
					source.hitchance, source.minDamage, source.minDamageOverride, source.minDamageOverrideValue,
					source.hitchanceOverride, source.hitchanceOverrideValue, source.pointScale,
					source.dynamicPointScale, static_cast<std::uint8_t>(source.hitboxes) };
			}
			value.aim = { Config::legit_aim.aimbot, Config::legit_aim.aimbot_fov, Config::legit_aim.team_check,
				Config::legit_aim.rcs, Config::legit_aim.fov_circle, copyColor(Config::legit_aim.fovCircleColor),
				Config::legit_aim.smoothing, Config::legit_aim.reactionDelay, Config::legit_aim.targetHeadBone,
				Config::legit_aim.visibilityCheck, Config::legit_aim.disableWhenFlashed,
				Config::legit_aim.onlyWhileScoped, Config::legit_aim.activationKey, Config::legit_aim.onKey,
				Config::legit_aim.smokeCheck };
            value.antiAim = { Config::anti_aim.enabled, Config::anti_aim.overridePitch, Config::anti_aim.overrideYaw,
                Config::anti_aim.pitchAmount, Config::anti_aim.yawAmount, Config::anti_aim.jitterAmount };
            value.movement.bunnyHop = Config::movement.bhop;
            value.movement.airStrafe = Config::movement.airStrafe;
			value.movement.slowWalk = Config::movement.slowWalk;
			value.movement.slowWalkScale = Config::movement.slowWalkScale;
			value.movement.quickPeek = Config::movement.quickPeek;
			value.movement.quickPeekKey = Config::movement.quickPeekKey;
			value.trigger = { Config::trigger.enabled, Config::trigger.dwellDelay, Config::trigger.headOnly,
				Config::trigger.teamCheck, Config::trigger.fov, Config::trigger.keybind, Config::trigger.onKey,
				Config::trigger.attackHold, Config::trigger.disableWhenFlashed, Config::trigger.onlyWhileScoped };
            value.feedback = { Config::feedback.hitMarker, Config::feedback.hitSound, Config::feedback.soundType,
                Config::feedback.soundVolume, Config::feedback.hitLogs, Config::feedback.bulletTracers,
                Config::feedback.bulletImpacts, copyColor(Config::feedback.tracerColor), copyColor(Config::feedback.impactColor),
                Config::feedback.separateHeadshotSound, Config::feedback.headshotSoundType };
            value.watermark = { Config::watermark.enabled, Config::watermark.position,
                Config::watermark.showFps, Config::watermark.showTime, Config::watermark.showVersion,
                Config::watermark.followAccent, copyColor(Config::watermark.color) };
            value.menu.accent = { vars::colorAccent[0], vars::colorAccent[1],
                vars::colorAccent[2], vars::colorAccent[3] };
            value.menu.rainbowTitle = vars::rainbowTitle;
            return value;
        }

        void publishLegacy(const tw::config::Settings& value)
        {
            Config::visual_esp = { value.esp.box, value.esp.health, value.esp.teamCheck, value.esp.fill, value.esp.thickness,
                value.esp.fillOpacity, copyColor(value.esp.color), value.esp.names, value.esp.flashed, value.esp.scoped,
                value.esp.skeleton, copyColor(value.esp.skeletonColor), value.esp.skeletonThickness, value.esp.weapon,
                value.esp.items, value.esp.grenades, value.esp.bomb };
			Config::visual_world = { value.world.night, copyColor(value.world.nightColor), value.world.customFov,
				value.world.fov, value.world.spectatorList, value.world.antiFlash,
				value.world.crosshair, value.world.velocityCounter };
			Config::visual_chams = { value.chams.enemyInvisible, value.chams.enemyVisible, value.chams.teamVisible,
                value.chams.teamInvisible, value.chams.local, value.chams.material, value.chams.arms, value.chams.viewmodel,
                copyColor(value.chams.viewmodelColor), copyColor(value.chams.armColor), copyColor(value.chams.enemyColor),
                copyColor(value.chams.enemyInvisibleColor), copyColor(value.chams.teamInvisibleColor),
				copyColor(value.chams.teamColor), copyColor(value.chams.localColor) };
			Config::rage.enabled = value.rage.enabled;
			Config::rage.selectedWeaponGroup = value.rage.selectedWeaponGroup;
			Config::rage.maxBacktrackTicks = value.rage.maxBacktrackTicks;
			Config::rage.extrapolation = value.rage.extrapolation;
			Config::rage.maxExtrapolateTicks = value.rage.maxExtrapolateTicks;
			for (std::size_t i = 0; i < value.rage.weaponGroups.size(); ++i)
			{
				const auto& source = value.rage.weaponGroups[i];
				auto& target = Config::rage.weaponGroups[i];
				target = { source.bodyAim, source.forceShotAir, source.forceShot, source.maxFov,
					source.hitchance, source.minDamage, source.minDamageOverride, source.minDamageOverrideValue,
					source.hitchanceOverride, source.hitchanceOverrideValue, source.pointScale,
					source.dynamicPointScale, source.hitboxes };
			}
			Config::legit_aim = { value.aim.enabled, value.aim.fov, value.aim.teamCheck, value.aim.recoilControl,
				value.aim.drawFov, copyColor(value.aim.fovColor), value.aim.smoothing,
				value.aim.reactionDelay, value.aim.targetHeadBone, value.aim.visibilityCheck,
				value.aim.disableWhenFlashed, value.aim.onlyWhileScoped, value.aim.activationKey, value.aim.onKey,
				value.aim.smokeCheck };
            Config::anti_aim = { value.antiAim.enabled, value.antiAim.overridePitch, value.antiAim.overrideYaw,
                value.antiAim.pitch, value.antiAim.yaw, value.antiAim.jitter };
            Config::movement.bhop = value.movement.bunnyHop;
            Config::movement.airStrafe = value.movement.airStrafe;
			Config::movement.slowWalk = value.movement.slowWalk;
			Config::movement.slowWalkScale = value.movement.slowWalkScale;
			Config::movement.quickPeek = value.movement.quickPeek;
			Config::movement.quickPeekKey = value.movement.quickPeekKey;
			Config::trigger = { value.trigger.enabled, value.trigger.dwellDelay, value.trigger.headOnly,
				value.trigger.teamCheck, value.trigger.fov, value.trigger.keybind, value.trigger.onKey,
				value.trigger.attackHold, value.trigger.disableWhenFlashed, value.trigger.onlyWhileScoped };
            Config::feedback = { value.feedback.hitMarker, value.feedback.hitSound, value.feedback.soundType,
                value.feedback.soundVolume, value.feedback.hitLogs, value.feedback.bulletTracers,
                value.feedback.bulletImpacts, copyColor(value.feedback.tracerColor), copyColor(value.feedback.impactColor),
                value.feedback.separateHeadshotSound, value.feedback.headshotSoundType };
            Config::watermark = { value.watermark.enabled, value.watermark.position,
                value.watermark.showFps, value.watermark.showTime, value.watermark.showVersion,
                value.watermark.followAccent, copyColor(value.watermark.color) };
            vars::rainbowTitle = value.menu.rainbowTitle;
            vars::colorAccent[0] = value.menu.accent.r;
            vars::colorAccent[1] = value.menu.accent.g;
            vars::colorAccent[2] = value.menu.accent.b;
            vars::colorAccent[3] = value.menu.accent.a;
        }

        tw::config::SettingsStore store{ configDirectory() };
        tw::config::SettingsRegistry registry{};
    }

    std::vector<std::string> ConfigManager::ListConfigs()
    {
        return store.list();
    }

    bool ConfigManager::Save(const std::string& configName) noexcept
    {
        const auto candidate = captureLegacy();
        if (!registry.commit(candidate))
            return false;
        return store.save(configName, candidate).ok();
    }

    bool ConfigManager::Load(const std::string& configName) noexcept
    {
        auto candidate = store.load(configName);
        if (!candidate || !registry.commit(candidate.value()))
            return false;
        publishLegacy(candidate.value());
        return true;
    }

    bool ConfigManager::Remove(const std::string& configName) noexcept
    {
        return store.remove(configName).ok();
    }

    bool ConfigManager::PublishCurrent() noexcept
    {
        return registry.commit(captureLegacy()).ok();
    }

    std::shared_ptr<const tw::config::Settings> ConfigManager::Snapshot() noexcept
    {
        return registry.snapshot();
    }
}
