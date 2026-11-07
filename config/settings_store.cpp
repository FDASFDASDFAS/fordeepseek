#include "settings_store.h"

#include "../../../external/json/json.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

namespace tw::config
{
    namespace
    {
        using Json = nlohmann::json;

        Json colorToJson(const Color4& value)
        {
            return Json::array({ value.r, value.g, value.b, value.a });
        }

        void readColor(const Json& object, const char* key, Color4& output)
        {
            const auto iterator = object.find(key);
            if (iterator == object.end())
                return;
            if (!iterator->is_array() || iterator->size() != 4)
                throw std::runtime_error("color must contain four components");
            output = { (*iterator)[0].get<float>(), (*iterator)[1].get<float>(),
                (*iterator)[2].get<float>(), (*iterator)[3].get<float>() };
        }

		template <typename T>
		void readValue(const Json& object, const char* key, T& output)
        {
            const auto iterator = object.find(key);
            if (iterator != object.end())
                output = iterator->get<T>();
		}

		void readRageWeaponGroup(const Json& object, RageSettings::WeaponGroup& output)
		{
			readValue(object, "body_aim", output.bodyAim);
			readValue(object, "force_shot_air", output.forceShotAir);
			readValue(object, "force_shot", output.forceShot);
			readValue(object, "max_fov", output.maxFov);
			readValue(object, "hitchance", output.hitchance);
			readValue(object, "minimum_damage", output.minDamage);
			readValue(object, "minimum_damage_override", output.minDamageOverride);
			readValue(object, "minimum_damage_override_value", output.minDamageOverrideValue);
			readValue(object, "hitchance_override", output.hitchanceOverride);
			readValue(object, "hitchance_override_value", output.hitchanceOverrideValue);
			readValue(object, "point_scale", output.pointScale);
			readValue(object, "dynamic_point_scale", output.dynamicPointScale);
			int hitboxes = output.hitboxes;
			readValue(object, "hitboxes", hitboxes);
			if (hitboxes < 0 || hitboxes > 0x3f)
				throw std::runtime_error("rage hitbox mask is out of range");
			output.hitboxes = static_cast<std::uint8_t>(hitboxes);
		}

		Json rageWeaponGroupToJson(const RageSettings::WeaponGroup& value)
		{
			return { { "body_aim", value.bodyAim }, { "force_shot_air", value.forceShotAir },
				{ "force_shot", value.forceShot }, { "max_fov", value.maxFov },
				{ "hitchance", value.hitchance }, { "minimum_damage", value.minDamage },
				{ "minimum_damage_override", value.minDamageOverride },
				{ "minimum_damage_override_value", value.minDamageOverrideValue },
				{ "hitchance_override", value.hitchanceOverride },
				{ "hitchance_override_value", value.hitchanceOverrideValue },
				{ "point_scale", value.pointScale }, { "dynamic_point_scale", value.dynamicPointScale },
				{ "hitboxes", value.hitboxes } };
		}

        Settings decodeSchemaZero(const Json& root)
        {
            Settings value{};
            readValue(root, "esp", value.esp.box);
            readValue(root, "showHealth", value.esp.health);
            readValue(root, "teamCheck", value.esp.teamCheck);
            readValue(root, "espFill", value.esp.fill);
            readValue(root, "espThickness", value.esp.thickness);
            readValue(root, "espFillOpacity", value.esp.fillOpacity);
            readColor(root, "espColor", value.esp.color);
            readValue(root, "showNameTags", value.esp.names);
            readValue(root, "showFlashed", value.esp.flashed);
            readValue(root, "showScoped", value.esp.scoped);
            readValue(root, "espSkeleton", value.esp.skeleton);
            readColor(root, "skeletonColor", value.esp.skeletonColor);
            readValue(root, "skeletonThickness", value.esp.skeletonThickness);

            readValue(root, "Night", value.world.night);
            readColor(root, "NightColor", value.world.nightColor);
            readValue(root, "fovEnabled", value.world.customFov);
            readValue(root, "fov", value.world.fov);
            readValue(root, "spectatorList", value.world.spectatorList);
            readValue(root, "antiflash", value.world.antiFlash);

            readValue(root, "enemyChams", value.chams.enemyVisible);
            readValue(root, "enemyChamsInvisible", value.chams.enemyInvisible);
            readValue(root, "teamChams", value.chams.teamVisible);
            readValue(root, "teamChamsInvisible", value.chams.teamInvisible);
            readValue(root, "localChams", value.chams.local);
            int material = value.chams.material;
            readValue(root, "chamsMaterial", material);
            value.chams.material = static_cast<std::uint8_t>(material);
            readValue(root, "armChams", value.chams.arms);
            readValue(root, "viewmodelChams", value.chams.viewmodel);
            readColor(root, "colVisualChams", value.chams.enemyColor);
            readColor(root, "colVisualChamsIgnoreZ", value.chams.enemyInvisibleColor);
            readColor(root, "teamcolVisualChams", value.chams.teamColor);
            readColor(root, "teamcolVisualChamsIgnoreZ", value.chams.teamInvisibleColor);
            readColor(root, "localcolChams", value.chams.localColor);
            readColor(root, "colArmChams", value.chams.armColor);
            readColor(root, "colViewmodelChams", value.chams.viewmodelColor);

            readValue(root, "aimbot", value.aim.enabled);
            readValue(root, "aimbot_fov", value.aim.fov);
            readValue(root, "team_check", value.aim.teamCheck);
            readValue(root, "rcs", value.aim.recoilControl);
            readValue(root, "fov_circle", value.aim.drawFov);
            readColor(root, "fovCircleColor", value.aim.fovColor);
            readValue(root, "anti_aim_enabled", value.antiAim.enabled);
            readValue(root, "anti_aim_overridePitch", value.antiAim.overridePitch);
            readValue(root, "anti_aim_overrideYaw", value.antiAim.overrideYaw);
            readValue(root, "anti_aim_pitchAmount", value.antiAim.pitch);
            readValue(root, "anti_aim_yawAmount", value.antiAim.yaw);
            readValue(root, "anti_aim_jitterAmount", value.antiAim.jitter);
            readValue(root, "bhop", value.movement.bunnyHop);
            readValue(root, "trigger", value.trigger.enabled);
            readValue(root, "trigger_dwell", value.trigger.dwellDelay);
            readValue(root, "trigger_head_only", value.trigger.headOnly);
            readValue(root, "trigger_team_check", value.trigger.teamCheck);
            readValue(root, "trigger_fov", value.trigger.fov);
            readValue(root, "trigger_key", value.trigger.keybind);
			readValue(root, "trigger_on_key", value.trigger.onKey);
			readValue(root, "ragebot", value.rage.enabled);
			return value;
        }

        Settings decodeSchemaOne(const Json& root)
        {
            Settings value{};
            const auto& visuals = root.at("visuals");
            const auto& esp = visuals.at("esp");
            const auto& world = visuals.at("world");
            const auto& chams = visuals.at("chams");
            const auto& combat = root.at("combat");
            const auto& aim = combat.at("aim");
            const auto& antiAim = combat.at("anti_aim");
			const auto& movement = root.at("movement");
			if (combat.contains("rage"))
			{
				const auto& rage = combat.at("rage");
				readValue(rage, "enabled", value.rage.enabled);
				readValue(rage, "selected_weapon_group", value.rage.selectedWeaponGroup);
				readValue(rage, "max_backtrack_ticks", value.rage.maxBacktrackTicks);
				readValue(rage, "extrapolation", value.rage.extrapolation);
				readValue(rage, "max_extrapolate_ticks", value.rage.maxExtrapolateTicks);
				if (const auto groups = rage.find("weapon_groups"); groups != rage.end() && groups->is_array())
				{
					const auto count = (std::min)(groups->size(), value.rage.weaponGroups.size());
					for (std::size_t i = 0; i < count; ++i)
						readRageWeaponGroup((*groups)[i], value.rage.weaponGroups[i]);
				}
			}

            readValue(esp, "box", value.esp.box);
            readValue(esp, "health", value.esp.health);
            readValue(esp, "team_check", value.esp.teamCheck);
            readValue(esp, "fill", value.esp.fill);
            readValue(esp, "thickness", value.esp.thickness);
            readValue(esp, "fill_opacity", value.esp.fillOpacity);
            readColor(esp, "color", value.esp.color);
            readValue(esp, "names", value.esp.names);
            readValue(esp, "flashed", value.esp.flashed);
            readValue(esp, "scoped", value.esp.scoped);
            readValue(esp, "skeleton", value.esp.skeleton);
            readColor(esp, "skeleton_color", value.esp.skeletonColor);
            readValue(esp, "skeleton_thickness", value.esp.skeletonThickness);
            readValue(esp, "weapon", value.esp.weapon);
            readValue(esp, "items", value.esp.items);
            readValue(esp, "grenades", value.esp.grenades);
            readValue(esp, "bomb", value.esp.bomb);

            readValue(world, "night", value.world.night);
            readColor(world, "night_color", value.world.nightColor);
            readValue(world, "custom_fov", value.world.customFov);
            readValue(world, "fov", value.world.fov);
			readValue(world, "spectator_list", value.world.spectatorList);
			readValue(world, "anti_flash", value.world.antiFlash);
			readValue(world, "crosshair", value.world.crosshair);
			readValue(world, "velocity_counter", value.world.velocityCounter);

            readValue(chams, "enemy_visible", value.chams.enemyVisible);
            readValue(chams, "enemy_invisible", value.chams.enemyInvisible);
            readValue(chams, "team_visible", value.chams.teamVisible);
            readValue(chams, "team_invisible", value.chams.teamInvisible);
            readValue(chams, "local", value.chams.local);
            int material = value.chams.material;
            readValue(chams, "material", material);
            value.chams.material = static_cast<std::uint8_t>(material);
            readValue(chams, "arms", value.chams.arms);
            readValue(chams, "viewmodel", value.chams.viewmodel);
            readColor(chams, "enemy_color", value.chams.enemyColor);
            readColor(chams, "enemy_invisible_color", value.chams.enemyInvisibleColor);
            readColor(chams, "team_color", value.chams.teamColor);
            readColor(chams, "team_invisible_color", value.chams.teamInvisibleColor);
            readColor(chams, "local_color", value.chams.localColor);
            readColor(chams, "arm_color", value.chams.armColor);
            readColor(chams, "viewmodel_color", value.chams.viewmodelColor);

            readValue(aim, "enabled", value.aim.enabled);
            readValue(aim, "fov", value.aim.fov);
            readValue(aim, "team_check", value.aim.teamCheck);
            readValue(aim, "recoil_control", value.aim.recoilControl);
            readValue(aim, "draw_fov", value.aim.drawFov);
            readColor(aim, "fov_color", value.aim.fovColor);
			readValue(aim, "smoothing", value.aim.smoothing);
			readValue(aim, "reaction_delay", value.aim.reactionDelay);
			readValue(aim, "target_head_bone", value.aim.targetHeadBone);
			readValue(aim, "visibility_check", value.aim.visibilityCheck);
			readValue(aim, "disable_when_flashed", value.aim.disableWhenFlashed);
			readValue(aim, "only_while_scoped", value.aim.onlyWhileScoped);
			readValue(aim, "activation_key", value.aim.activationKey);
			readValue(aim, "on_key", value.aim.onKey);
			readValue(aim, "smoke_check", value.aim.smokeCheck);
            readValue(antiAim, "enabled", value.antiAim.enabled);
            readValue(antiAim, "override_pitch", value.antiAim.overridePitch);
            readValue(antiAim, "override_yaw", value.antiAim.overrideYaw);
            readValue(antiAim, "pitch", value.antiAim.pitch);
            readValue(antiAim, "yaw", value.antiAim.yaw);
            readValue(antiAim, "jitter", value.antiAim.jitter);
            readValue(movement, "bunny_hop", value.movement.bunnyHop);
            readValue(movement, "air_strafe", value.movement.airStrafe);
			readValue(movement, "slow_walk", value.movement.slowWalk);
			readValue(movement, "slow_walk_scale", value.movement.slowWalkScale);
			readValue(movement, "quick_peek", value.movement.quickPeek);
			readValue(movement, "quick_peek_key", value.movement.quickPeekKey);
            const auto& trigger = root.at("trigger");
            readValue(trigger, "enabled", value.trigger.enabled);
            readValue(trigger, "dwell_delay", value.trigger.dwellDelay);
            readValue(trigger, "attack_hold", value.trigger.attackHold);
            readValue(trigger, "head_only", value.trigger.headOnly);
            readValue(trigger, "team_check", value.trigger.teamCheck);
            readValue(trigger, "fov", value.trigger.fov);
            readValue(trigger, "keybind", value.trigger.keybind);
			readValue(trigger, "on_key", value.trigger.onKey);
			readValue(trigger, "disable_when_flashed", value.trigger.disableWhenFlashed);
			readValue(trigger, "only_while_scoped", value.trigger.onlyWhileScoped);
            if (root.contains("feedback")) {
                const auto& feedback = root.at("feedback");
                readValue(feedback, "hit_marker", value.feedback.hitMarker);
                readValue(feedback, "hit_sound", value.feedback.hitSound);
                readValue(feedback, "sound_type", value.feedback.soundType);
                readValue(feedback, "separate_headshot_sound", value.feedback.separateHeadshotSound);
                readValue(feedback, "headshot_sound_type", value.feedback.headshotSoundType);
                readValue(feedback, "sound_volume", value.feedback.soundVolume);
                readValue(feedback, "hit_logs", value.feedback.hitLogs);
                readValue(feedback, "bullet_tracers", value.feedback.bulletTracers);
                readValue(feedback, "bullet_impacts", value.feedback.bulletImpacts);
                readColor(feedback, "tracer_color", value.feedback.tracerColor);
                readColor(feedback, "impact_color", value.feedback.impactColor);
            }
            // Both sections post-date schema 2, so a config written before them
            // simply keeps the defaults rather than failing to decode.
            if (root.contains("watermark")) {
                const auto& watermark = root.at("watermark");
                readValue(watermark, "enabled", value.watermark.enabled);
                readValue(watermark, "position", value.watermark.position);
                readValue(watermark, "show_fps", value.watermark.showFps);
                readValue(watermark, "show_time", value.watermark.showTime);
                readValue(watermark, "show_version", value.watermark.showVersion);
                readValue(watermark, "follow_accent", value.watermark.followAccent);
                readColor(watermark, "color", value.watermark.color);
            }
            if (root.contains("menu")) {
                readColor(root.at("menu"), "accent", value.menu.accent);
                readValue(root.at("menu"), "rainbow_title", value.menu.rainbowTitle);
            }
            return value;
        }

        std::wstring wide(const std::filesystem::path& path)
        {
            return path.wstring();
        }
    }

    bool validProfileName(const std::string_view name) noexcept
    {
        if (name.empty() || name.size() > 64 || name == "." || name == ".." || name.back() == '.' || name.back() == ' ')
            return false;
        for (const unsigned char character : name)
        {
            if (!(std::isalnum(character) || character == '_' || character == '-' || character == '.'))
                return false;
        }
        std::string upper{ name };
        std::transform(upper.begin(), upper.end(), upper.begin(), [](const unsigned char character) { return static_cast<char>(std::toupper(character)); });
        const auto dot = upper.find('.');
        const auto base = upper.substr(0, dot);
        static constexpr std::array reserved{ "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
            "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
        return std::find(reserved.begin(), reserved.end(), base) == reserved.end();
    }

    runtime::Result<Settings> decodeSettings(const std::string_view jsonText) noexcept
    {
        try
        {
            const Json root = Json::parse(jsonText);
            if (!root.is_object())
                return runtime::Result<Settings>::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 20, 0 });
            const std::uint32_t schema = root.value("schema_version", 0U);
            Settings settings{};
            if (schema == 0)
                settings = decodeSchemaZero(root);
			else if (schema == 1 || schema == currentSchemaVersion)
				settings = decodeSchemaOne(root);
            else
                return runtime::Result<Settings>::failure({ runtime::StatusCode::Unsupported, runtime::Subsystem::Configuration, 21, static_cast<long>(schema) });
            const auto status = validate(settings);
            if (!status)
                return runtime::Result<Settings>::failure(status.error());
            return runtime::Result<Settings>::success(std::move(settings));
        }
        catch (...)
        {
            return runtime::Result<Settings>::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 22, 0 });
        }
    }

    runtime::Result<std::string> encodeSettings(const Settings& value) noexcept
    {
        const auto status = validate(value);
        if (!status)
            return runtime::Result<std::string>::failure(status.error());
        try
        {
            Json root;
            root["schema_version"] = currentSchemaVersion;
            auto& esp = root["visuals"]["esp"];
            esp = { { "box", value.esp.box }, { "health", value.esp.health }, { "team_check", value.esp.teamCheck },
                { "fill", value.esp.fill }, { "thickness", value.esp.thickness }, { "fill_opacity", value.esp.fillOpacity },
                { "color", colorToJson(value.esp.color) }, { "names", value.esp.names }, { "flashed", value.esp.flashed },
                { "scoped", value.esp.scoped }, { "skeleton", value.esp.skeleton },
                { "skeleton_color", colorToJson(value.esp.skeletonColor) }, { "skeleton_thickness", value.esp.skeletonThickness },
                { "weapon", value.esp.weapon }, { "items", value.esp.items }, { "grenades", value.esp.grenades },
                { "bomb", value.esp.bomb } };
            auto& world = root["visuals"]["world"];
			world = { { "night", value.world.night }, { "night_color", colorToJson(value.world.nightColor) },
				{ "custom_fov", value.world.customFov }, { "fov", value.world.fov },
				{ "spectator_list", value.world.spectatorList }, { "anti_flash", value.world.antiFlash },
				{ "crosshair", value.world.crosshair }, { "velocity_counter", value.world.velocityCounter } };
            auto& chams = root["visuals"]["chams"];
            chams = { { "enemy_visible", value.chams.enemyVisible }, { "enemy_invisible", value.chams.enemyInvisible },
                { "team_visible", value.chams.teamVisible }, { "team_invisible", value.chams.teamInvisible },
                { "local", value.chams.local }, { "material", value.chams.material }, { "arms", value.chams.arms },
                { "viewmodel", value.chams.viewmodel }, { "enemy_color", colorToJson(value.chams.enemyColor) },
                { "enemy_invisible_color", colorToJson(value.chams.enemyInvisibleColor) },
                { "team_color", colorToJson(value.chams.teamColor) },
                { "team_invisible_color", colorToJson(value.chams.teamInvisibleColor) },
                { "local_color", colorToJson(value.chams.localColor) }, { "arm_color", colorToJson(value.chams.armColor) },
                { "viewmodel_color", colorToJson(value.chams.viewmodelColor) } };
			root["combat"]["aim"] = { { "enabled", value.aim.enabled }, { "fov", value.aim.fov },
                { "team_check", value.aim.teamCheck }, { "recoil_control", value.aim.recoilControl },
				{ "draw_fov", value.aim.drawFov }, { "fov_color", colorToJson(value.aim.fovColor) },
				{ "smoothing", value.aim.smoothing }, { "reaction_delay", value.aim.reactionDelay },
				{ "target_head_bone", value.aim.targetHeadBone },
				{ "visibility_check", value.aim.visibilityCheck },
				{ "disable_when_flashed", value.aim.disableWhenFlashed },
				{ "only_while_scoped", value.aim.onlyWhileScoped },
				{ "activation_key", value.aim.activationKey }, { "on_key", value.aim.onKey },
				{ "smoke_check", value.aim.smokeCheck } };
			auto& rage = root["combat"]["rage"];
			rage = { { "enabled", value.rage.enabled },
				{ "selected_weapon_group", value.rage.selectedWeaponGroup },
				{ "max_backtrack_ticks", value.rage.maxBacktrackTicks },
				{ "extrapolation", value.rage.extrapolation },
				{ "max_extrapolate_ticks", value.rage.maxExtrapolateTicks } };
			rage["weapon_groups"] = Json::array();
			for (const auto& group : value.rage.weaponGroups)
				rage["weapon_groups"].push_back(rageWeaponGroupToJson(group));
            root["combat"]["anti_aim"] = { { "enabled", value.antiAim.enabled }, { "override_pitch", value.antiAim.overridePitch },
                { "override_yaw", value.antiAim.overrideYaw }, { "pitch", value.antiAim.pitch },
                { "yaw", value.antiAim.yaw }, { "jitter", value.antiAim.jitter } };
			root["movement"] = { { "bunny_hop", value.movement.bunnyHop }, { "air_strafe", value.movement.airStrafe },
				{ "slow_walk", value.movement.slowWalk }, { "slow_walk_scale", value.movement.slowWalkScale },
				{ "quick_peek", value.movement.quickPeek }, { "quick_peek_key", value.movement.quickPeekKey } };
			root["trigger"] = { { "enabled", value.trigger.enabled }, { "dwell_delay", value.trigger.dwellDelay },
				{ "attack_hold", value.trigger.attackHold },
				{ "head_only", value.trigger.headOnly }, { "team_check", value.trigger.teamCheck },
				{ "fov", value.trigger.fov }, { "keybind", value.trigger.keybind }, { "on_key", value.trigger.onKey },
				{ "disable_when_flashed", value.trigger.disableWhenFlashed },
				{ "only_while_scoped", value.trigger.onlyWhileScoped } };
            root["feedback"] = { { "hit_marker", value.feedback.hitMarker }, { "hit_sound", value.feedback.hitSound },
                { "sound_type", value.feedback.soundType }, { "sound_volume", value.feedback.soundVolume },
                { "separate_headshot_sound", value.feedback.separateHeadshotSound },
                { "headshot_sound_type", value.feedback.headshotSoundType },
                { "hit_logs", value.feedback.hitLogs }, { "bullet_tracers", value.feedback.bulletTracers },
                { "bullet_impacts", value.feedback.bulletImpacts }, { "tracer_color", colorToJson(value.feedback.tracerColor) },
                { "impact_color", colorToJson(value.feedback.impactColor) } };
            root["watermark"] = { { "enabled", value.watermark.enabled }, { "position", value.watermark.position },
                { "show_fps", value.watermark.showFps }, { "show_time", value.watermark.showTime },
                { "show_version", value.watermark.showVersion }, { "follow_accent", value.watermark.followAccent },
                { "color", colorToJson(value.watermark.color) } };
            root["menu"] = { { "accent", colorToJson(value.menu.accent) },
                { "rainbow_title", value.menu.rainbowTitle } };
            return runtime::Result<std::string>::success(root.dump(4));
        }
        catch (...)
        {
            return runtime::Result<std::string>::failure({ runtime::StatusCode::RuntimeFailure, runtime::Subsystem::Configuration, 23, 0 });
        }
    }

    runtime::Result<Settings> SettingsStore::load(const std::string_view profileName) const noexcept
    {
        if (!validProfileName(profileName))
            return runtime::Result<Settings>::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 24, 0 });
        try
        {
            const auto path = pathFor(profileName);
            std::ifstream stream{ path, std::ios::binary };
            if (!stream)
                return runtime::Result<Settings>::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 25, 0 });
            stream.seekg(0, std::ios::end);
            const auto size = stream.tellg();
            if (size < 0 || size > 1024 * 1024)
                return runtime::Result<Settings>::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 26, 0 });
            stream.seekg(0, std::ios::beg);
            const std::string text{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{} };
            if (!stream.good() && !stream.eof())
                return runtime::Result<Settings>::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 27, 0 });
            return decodeSettings(text);
        }
        catch (...)
        {
            return runtime::Result<Settings>::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 28, 0 });
        }
    }

    runtime::Status SettingsStore::save(const std::string_view profileName, const Settings& settings) const noexcept
    {
        if (!validProfileName(profileName))
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 29, 0 });
        auto encoded = encodeSettings(settings);
        if (!encoded)
            return runtime::Status::failure(encoded.error());
        try
        {
            std::error_code error;
            std::filesystem::create_directories(directory_, error);
            if (error)
                return runtime::Status::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 30, error.value() });
            const auto target = pathFor(profileName);
            const auto temporary = target.wstring() + L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
            HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return runtime::Status::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 31, static_cast<long>(GetLastError()) });
            DWORD written{};
            const auto& text = encoded.value();
            const BOOL wrote = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
            const BOOL flushed = wrote && written == text.size() && FlushFileBuffers(file);
            const DWORD writeError = flushed ? ERROR_SUCCESS : GetLastError();
            CloseHandle(file);
            if (!flushed)
            {
                DeleteFileW(temporary.c_str());
                return runtime::Status::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 32, static_cast<long>(writeError) });
            }

            BOOL replaced = FALSE;
            if (std::filesystem::exists(target, error) && !error)
                replaced = ReplaceFileW(wide(target).c_str(), temporary.c_str(), nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr);
            else
                replaced = MoveFileExW(temporary.c_str(), wide(target).c_str(), MOVEFILE_WRITE_THROUGH);
            if (!replaced)
            {
                const DWORD replaceError = GetLastError();
                DeleteFileW(temporary.c_str());
                return runtime::Status::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 33, static_cast<long>(replaceError) });
            }
            return runtime::Status::success();
        }
        catch (...)
        {
            return runtime::Status::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 34, 0 });
        }
    }

    runtime::Status SettingsStore::remove(const std::string_view profileName) const noexcept
    {
        if (!validProfileName(profileName))
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 35, 0 });
        std::error_code error;
        std::filesystem::remove(pathFor(profileName), error);
        return error ? runtime::Status::failure({ runtime::StatusCode::IoFailure, runtime::Subsystem::Configuration, 36, error.value() })
                     : runtime::Status::success();
    }

    std::vector<std::string> SettingsStore::list() const noexcept
    {
        std::vector<std::string> result;
        std::error_code error;
        if (!std::filesystem::exists(directory_, error) || error)
            return result;
        for (std::filesystem::directory_iterator iterator{ directory_, error }, end; !error && iterator != end; iterator.increment(error))
        {
            if (iterator->is_regular_file(error) && !error && iterator->path().extension() == ".json")
                result.push_back(iterator->path().stem().string());
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    std::filesystem::path SettingsStore::pathFor(const std::string_view profileName) const
    {
        return directory_ / (std::string{ profileName } + ".json");
    }
}
