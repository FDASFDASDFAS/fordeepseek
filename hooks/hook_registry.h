#pragma once

#include "../runtime/result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tw::hooks
{
    class IHookBackend
    {
    public:
        virtual ~IHookBackend() = default;
        [[nodiscard]] virtual runtime::Result<void*> create(void* target, void* detour) noexcept = 0;
        [[nodiscard]] virtual runtime::Status enable(void* target) noexcept = 0;
        [[nodiscard]] virtual runtime::Status disable(void* target) noexcept = 0;
        [[nodiscard]] virtual runtime::Status remove(void* target) noexcept = 0;
    };

    enum class RegistrationState : std::uint8_t
    {
        Empty,
        Created,
        Enabled,
        Disabled,
        Removed,
        Failed
    };

    struct HookSpec final
    {
        std::string name{};
        void* target{};
        void* detour{};
        void** originalSlot{};
    };

    class HookRegistration final
    {
    public:
        HookRegistration() = default;
        explicit HookRegistration(HookSpec spec);
        HookRegistration(HookRegistration&& other) noexcept;
        HookRegistration& operator=(HookRegistration&& other) noexcept;

        HookRegistration(const HookRegistration&) = delete;
        HookRegistration& operator=(const HookRegistration&) = delete;

        [[nodiscard]] const HookSpec& spec() const noexcept { return spec_; }
        [[nodiscard]] RegistrationState state() const noexcept { return state_; }

    private:
        friend class HookRegistry;
        HookSpec spec_{};
        RegistrationState state_{ RegistrationState::Empty };
    };

    class HookRegistry final
    {
    public:
        explicit HookRegistry(IHookBackend& backend) noexcept : backend_(backend) {}

        [[nodiscard]] runtime::Status createAll(std::vector<HookSpec> specs);
        [[nodiscard]] runtime::Status enableAll() noexcept;
        [[nodiscard]] runtime::Status disableAll() noexcept;
        [[nodiscard]] runtime::Status removeAll() noexcept;
        [[nodiscard]] std::size_t size() const noexcept { return registrations_.size(); }
        [[nodiscard]] bool hasActiveRegistrations() const noexcept;

    private:
        [[nodiscard]] runtime::Status rollbackCreated() noexcept;

        IHookBackend& backend_;
        std::vector<HookRegistration> registrations_{};
    };
}
