#include "settings.h"

#include <cmath>
#include <utility>

namespace tw::config
{
    namespace
    {
        bool validColor(const Color4& value) noexcept
        {
            const auto channel = [](const float component) { return std::isfinite(component) && component >= 0.0f && component <= 1.0f; };
            return channel(value.r) && channel(value.g) && channel(value.b) && channel(value.a);
        }
    }

    runtime::Status validate(const Settings& settings) noexcept
    {
        if (settings.schemaVersion != currentSchemaVersion)
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 1, 0 });
        if (!std::isfinite(settings.esp.thickness) || settings.esp.thickness < 1.0f || settings.esp.thickness > 5.0f ||
            !std::isfinite(settings.esp.skeletonThickness) || settings.esp.skeletonThickness < 1.0f || settings.esp.skeletonThickness > 5.0f ||
            !std::isfinite(settings.esp.fillOpacity) || settings.esp.fillOpacity < 0.0f || settings.esp.fillOpacity > 1.0f)
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 2, 0 });
		if (!std::isfinite(settings.world.fov) || settings.world.fov < 20.0f || settings.world.fov > 160.0f ||
			!std::isfinite(settings.aim.fov) || settings.aim.fov < 0.0f || settings.aim.fov > 90.0f ||
			!std::isfinite(settings.aim.smoothing) || settings.aim.smoothing < 1.0f || settings.aim.smoothing > 30.0f ||
			!std::isfinite(settings.aim.reactionDelay) || settings.aim.reactionDelay < 0.0f || settings.aim.reactionDelay > 1.0f ||
			settings.aim.activationKey < 0 || settings.aim.activationKey > 0xff)
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 3, 0 });
        if (settings.chams.material > 2 || settings.antiAim.pitch < -89 || settings.antiAim.pitch > 89 ||
            settings.antiAim.yaw < 0 || settings.antiAim.yaw > 180 || settings.antiAim.jitter < 0 || settings.antiAim.jitter > 180)
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 4, 0 });
		if (!std::isfinite(settings.movement.slowWalkScale) || settings.movement.slowWalkScale < 0.0f || settings.movement.slowWalkScale > 1.0f ||
			settings.movement.quickPeekKey < 0 || settings.movement.quickPeekKey > 0xff)
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 5, 0 });
		if (!std::isfinite(settings.trigger.dwellDelay) || settings.trigger.dwellDelay < 0.0f || settings.trigger.dwellDelay > 1.0f ||
			!std::isfinite(settings.trigger.fov) || settings.trigger.fov < 1.0f || settings.trigger.fov > 30.0f ||
			settings.trigger.keybind < 0 || settings.trigger.keybind > 0xff)
			return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 7, 0 });
		if (settings.rage.selectedWeaponGroup < 0 || settings.rage.selectedWeaponGroup >= static_cast<int>(settings.rage.weaponGroups.size()) ||
			settings.rage.maxBacktrackTicks < 1 || settings.rage.maxBacktrackTicks > 16 ||
			settings.rage.maxExtrapolateTicks < 1 || settings.rage.maxExtrapolateTicks > 16)
			return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 8, 0 });
		for (const auto& group : settings.rage.weaponGroups)
		{
			if (!std::isfinite(group.maxFov) || group.maxFov < 1.0f || group.maxFov > 180.0f ||
				group.hitchance < 0 || group.hitchance > 100 || group.minDamage < 1 || group.minDamage > 125 ||
				group.minDamageOverrideValue < 0 || group.minDamageOverrideValue > 130 ||
				group.hitchanceOverrideValue < 0 || group.hitchanceOverrideValue > 100 ||
				!std::isfinite(group.pointScale) || group.pointScale < 0.0f || group.pointScale > 100.0f ||
				group.hitboxes > 0x3f)
			{
				return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 9, 0 });
			}
		}
        if (settings.watermark.position < 0 || settings.watermark.position > 3)
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 10, 0 });
        if (!validColor(settings.esp.color) || !validColor(settings.esp.skeletonColor) || !validColor(settings.world.nightColor) ||
            !validColor(settings.chams.enemyColor) || !validColor(settings.chams.enemyInvisibleColor) ||
            !validColor(settings.chams.teamColor) || !validColor(settings.chams.teamInvisibleColor) ||
            !validColor(settings.chams.localColor) || !validColor(settings.chams.armColor) ||
            !validColor(settings.chams.viewmodelColor) || !validColor(settings.aim.fovColor) ||
            !validColor(settings.menu.accent) || !validColor(settings.watermark.color))
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Configuration, 6, 0 });
        return runtime::Status::success();
    }

    SettingsRegistry::SettingsRegistry() : current_(std::make_shared<const Settings>()) {}

    std::shared_ptr<const Settings> SettingsRegistry::snapshot() const noexcept
    {
        std::scoped_lock lock{ mutex_ };
        return current_;
    }

    runtime::Status SettingsRegistry::commit(Settings candidate)
    {
        const auto status = validate(candidate);
        if (!status)
            return status;
        auto replacement = std::make_shared<const Settings>(std::move(candidate));
        std::scoped_lock lock{ mutex_ };
        current_ = std::move(replacement);
        return runtime::Status::success();
    }
}
