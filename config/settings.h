#pragma once

#include "../runtime/result.h"

#include <cstdint>
#include <array>
#include <memory>
#include <mutex>

namespace tw::config
{
	inline constexpr std::uint32_t currentSchemaVersion = 2;
    inline constexpr float minimumSlowWalkScale = 0.50f;

    struct Color4 final
    {
        float r{ 1.0f };
        float g{};
        float b{};
        float a{ 1.0f };
    };

    struct EspSettings final
    {
        bool box{};
        bool health{};
		bool teamCheck{ true };
        bool fill{};
        float thickness{ 1.0f };
        float fillOpacity{ 0.5f };
        Color4 color{};
        bool names{};
        bool flashed{};
        bool scoped{};
        bool skeleton{};
        Color4 skeletonColor{};
        float skeletonThickness{ 1.0f };
        bool weapon{};
        bool items{};
        bool grenades{};
        bool bomb{};
    };

    struct WorldSettings final
    {
        bool night{};
        Color4 nightColor{ 0.1f, 0.1f, 0.1f, 1.0f };
        bool customFov{};
        float fov{ 90.0f };
		bool spectatorList{};
		bool antiFlash{};
		bool crosshair{};
		bool velocityCounter{};
    };

    struct ChamsSettings final
    {
        bool enemyVisible{};
        bool enemyInvisible{};
        bool teamVisible{};
        bool teamInvisible{};
        bool local{};
        std::uint8_t material{};
        bool arms{};
        bool viewmodel{};
        Color4 enemyColor{};
        Color4 enemyInvisibleColor{};
        Color4 teamColor{};
        Color4 teamInvisibleColor{};
        Color4 localColor{};
        Color4 armColor{};
        Color4 viewmodelColor{};
    };

	struct AimSettings final
    {
        bool enabled{};
        float fov{};
        bool teamCheck{};
        bool recoilControl{};
        bool drawFov{};
		Color4 fovColor{};
		float smoothing{ 1.0f };
		float reactionDelay{};
		bool targetHeadBone{};
		bool visibilityCheck{ true };
		bool disableWhenFlashed{ true };
		bool onlyWhileScoped{};
		int activationKey{ 0x06 };
		bool onKey{ true };
		// Refuse a target the shot would have to cross a smoke cloud to reach.
		// Independent of visibilityCheck: the engine trace passes straight
		// through smoke, so it cannot answer this on its own.
		bool smokeCheck{ true };
	};

	struct RageSettings final
	{
		bool enabled{};
		int selectedWeaponGroup{};
		int maxBacktrackTicks{ 12 };
		bool extrapolation{ true };
		int maxExtrapolateTicks{ 8 };

		struct WeaponGroup final
		{
			bool bodyAim{};
			bool forceShotAir{};
			bool forceShot{};
			float maxFov{ 180.0f };
			int hitchance{ 80 };
			int minDamage{ 1 };
			bool minDamageOverride{};
			int minDamageOverrideValue{ 11 };
			bool hitchanceOverride{};
			int hitchanceOverrideValue{ 75 };
			float pointScale{ 85.0f };
			bool dynamicPointScale{ true };
			std::uint8_t hitboxes{ 0x3f };
		};

		std::array<WeaponGroup, 6> weaponGroups{};
	};

    struct AntiAimSettings final
    {
        bool enabled{};
        bool overridePitch{};
        bool overrideYaw{};
        int pitch{};
        int yaw{};
        int jitter{};
    };

    struct MovementSettings final
    {
        bool bunnyHop{};
        bool airStrafe{};
		bool slowWalk{};
		float slowWalkScale{ minimumSlowWalkScale };
		bool quickPeek{};
		int quickPeekKey{ 'V' };
    };

    struct TriggerSettings final
    {
        bool enabled{};
        float dwellDelay{ 0.05f };
        bool headOnly{};
        bool teamCheck{ true };
        float fov{ 5.0f };
		int keybind{ 0x05 };
		bool onKey{ true };
		float attackHold{ 0.03f };
		bool disableWhenFlashed{ true };
		bool onlyWhileScoped{};
    };

    struct FeedbackSettings final
    {
        bool hitMarker{ true };
        bool hitSound{ true };
        int soundType{ 0 };
        float soundVolume{ 0.8f };
        bool hitLogs{ true };
        bool bulletTracers{ true };
        bool bulletImpacts{ true };
        Color4 tracerColor{ 0.2f, 0.8f, 1.0f, 0.8f };
        Color4 impactColor{ 1.0f, 0.3f, 0.3f, 0.8f };
        bool separateHeadshotSound{ false };
        int headshotSoundType{ 0 };
    };

    struct WatermarkSettings final
    {
        bool enabled{ true };
        // 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right.
        int position{};
        bool showFps{ true };
        bool showTime{ true };
        bool showVersion{ true };
        bool followAccent{ true };
        Color4 color{ 0.263f, 0.717f, 0.898f, 1.0f };
    };

    struct MenuSettings final
    {
        // Must match the compiled-in defaults in menu/gui/styling.h, so a config
        // written before this section existed does not recolour the menu.
        Color4 accent{ 0.263f, 0.717f, 0.898f, 1.0f };
        bool rainbowTitle{ true };
    };

    struct Settings final
    {
        std::uint32_t schemaVersion{ currentSchemaVersion };
        EspSettings esp{};
        WorldSettings world{};
		ChamsSettings chams{};
		RageSettings rage{};
		AimSettings aim{};
        AntiAimSettings antiAim{};
        MovementSettings movement{};
        TriggerSettings trigger{};
        FeedbackSettings feedback{};
        WatermarkSettings watermark{};
        MenuSettings menu{};
    };

    [[nodiscard]] runtime::Status validate(const Settings& settings) noexcept;

    class SettingsRegistry final
    {
    public:
        SettingsRegistry();
        [[nodiscard]] std::shared_ptr<const Settings> snapshot() const noexcept;
        [[nodiscard]] runtime::Status commit(Settings candidate);

    private:
        mutable std::mutex mutex_{};
        std::shared_ptr<const Settings> current_{};
    };
}
