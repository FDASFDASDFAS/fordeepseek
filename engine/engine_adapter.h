#pragma once

#include "../entities/entity_state.h"
#include "../runtime/result.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace tw::engine
{
    enum class Capability : std::uint8_t
    {
        CommandCaptureCommit,
        EntityGeneration,
        SnapshotPhase,
        SemanticPose,
        Projection,
        VisibilityTrace,
        HullTrace,
        PenetrationDamage,
        WeaponObservation,
        Prediction,
        TypedEvents,
        SceneStyle,
        WorldStyle,
        Camera,
        ThrowablePhysics,
        Purchase,
        CosmeticCatalog,
        CosmeticMutation,
        LocalAssets,
        ResizeBuffers,
        LoaderRelease,
        Count
    };

    struct CapabilityState final
    {
        runtime::StatusCode status{ runtime::StatusCode::Unsupported };
        std::string_view reason{};
    };

    class CapabilityReport final
    {
    public:
        CapabilityReport() noexcept
        {
            for (auto& capability : capabilities_)
                capability = { runtime::StatusCode::Unsupported, "current runtime contract not established" };
        }

        void set(Capability capability, CapabilityState state) noexcept
        {
            capabilities_[static_cast<std::size_t>(capability)] = state;
        }

        [[nodiscard]] CapabilityState get(Capability capability) const noexcept
        {
            return capabilities_[static_cast<std::size_t>(capability)];
        }

        [[nodiscard]] bool available(Capability capability) const noexcept
        {
            return get(capability).status == runtime::StatusCode::Ok;
        }

    private:
        std::array<CapabilityState, static_cast<std::size_t>(Capability::Count)> capabilities_{};
    };

    struct TraceQuery final
    {
        entities::Vec3 start{};
        entities::Vec3 end{};
        entities::EntityKey ignoredEntity{};
    };

    struct TraceResult final
    {
        float fraction{ 1.0f };
        entities::Vec3 endPosition{};
        std::optional<entities::EntityKey> hitEntity{};
    };

    class ITraceService
    {
    public:
        virtual ~ITraceService() = default;
        [[nodiscard]] virtual runtime::Result<TraceResult> visibility(const TraceQuery& query) noexcept = 0;
        [[nodiscard]] virtual runtime::Result<TraceResult> hull(const TraceQuery& query, entities::Vec3 extents) noexcept = 0;
    };

    class EngineAdapter
    {
    public:
        virtual ~EngineAdapter() = default;
        [[nodiscard]] virtual const CapabilityReport& capabilities() const noexcept = 0;
        [[nodiscard]] virtual runtime::Status refreshSession() noexcept = 0;
    };
}
