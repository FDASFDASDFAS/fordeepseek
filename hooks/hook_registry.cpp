#include "hook_registry.h"

#include <algorithm>
#include <utility>

namespace tw::hooks
{
    HookRegistration::HookRegistration(HookSpec spec) : spec_(std::move(spec)) {}

    HookRegistration::HookRegistration(HookRegistration&& other) noexcept
        : spec_(std::move(other.spec_)), state_(std::exchange(other.state_, RegistrationState::Empty))
    {
    }

    HookRegistration& HookRegistration::operator=(HookRegistration&& other) noexcept
    {
        if (this != &other)
        {
            spec_ = std::move(other.spec_);
            state_ = std::exchange(other.state_, RegistrationState::Empty);
        }
        return *this;
    }

    runtime::Status HookRegistry::createAll(std::vector<HookSpec> specs)
    {
        if (!registrations_.empty())
            return runtime::Status::failure({ runtime::StatusCode::InvalidState, runtime::Subsystem::Hooks, 1, 0 });

        for (std::size_t index = 0; index < specs.size(); ++index)
        {
            const auto& spec = specs[index];
            if (spec.name.empty() || spec.target == nullptr || spec.detour == nullptr || spec.originalSlot == nullptr)
                return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Hooks, 2, static_cast<long>(index) });
            const auto duplicate = std::find_if(specs.begin(), specs.begin() + static_cast<std::ptrdiff_t>(index),
                [&spec](const HookSpec& candidate) { return candidate.target == spec.target; });
            if (duplicate != specs.begin() + static_cast<std::ptrdiff_t>(index))
                return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Hooks, 3, static_cast<long>(index) });
        }

        registrations_.reserve(specs.size());
        for (auto& spec : specs)
            registrations_.emplace_back(std::move(spec));

        for (auto& registration : registrations_)
        {
            auto created = backend_.create(registration.spec_.target, registration.spec_.detour);
            if (!created)
            {
                registration.state_ = RegistrationState::Failed;
                const auto cleanup = rollbackCreated();
                if (!cleanup)
                    return cleanup;
                registrations_.clear();
                return runtime::Status::failure(created.error());
            }
            *registration.spec_.originalSlot = created.value();
            registration.state_ = RegistrationState::Created;
        }
        return runtime::Status::success();
    }

    runtime::Status HookRegistry::enableAll() noexcept
    {
        for (auto& registration : registrations_)
        {
            if (registration.state_ != RegistrationState::Created && registration.state_ != RegistrationState::Disabled)
                return runtime::Status::failure({ runtime::StatusCode::InvalidState, runtime::Subsystem::Hooks, 4, 0 });
            const auto result = backend_.enable(registration.spec_.target);
            if (!result)
            {
                registration.state_ = RegistrationState::Failed;
                const auto cleanup = disableAll();
                return cleanup ? result : cleanup;
            }
            registration.state_ = RegistrationState::Enabled;
        }
        return runtime::Status::success();
    }

    runtime::Status HookRegistry::disableAll() noexcept
    {
        runtime::Error firstError{};
        for (auto iterator = registrations_.rbegin(); iterator != registrations_.rend(); ++iterator)
        {
            if (iterator->state_ != RegistrationState::Enabled)
                continue;
            const auto result = backend_.disable(iterator->spec_.target);
            if (!result)
            {
                iterator->state_ = RegistrationState::Failed;
                if (firstError.code == runtime::StatusCode::Ok)
                    firstError = { runtime::StatusCode::UnsafeTeardown, runtime::Subsystem::Hooks, 5, result.error().nativeCode };
                continue;
            }
            iterator->state_ = RegistrationState::Disabled;
        }
        return firstError.code == runtime::StatusCode::Ok ? runtime::Status::success() : runtime::Status::failure(firstError);
    }

    runtime::Status HookRegistry::removeAll() noexcept
    {
        if (hasActiveRegistrations())
            return runtime::Status::failure({ runtime::StatusCode::UnsafeTeardown, runtime::Subsystem::Hooks, 6, 0 });

        runtime::Error firstError{};
        for (auto iterator = registrations_.rbegin(); iterator != registrations_.rend(); ++iterator)
        {
            if (iterator->state_ == RegistrationState::Removed || iterator->state_ == RegistrationState::Empty)
                continue;
            const auto result = backend_.remove(iterator->spec_.target);
            if (!result)
            {
                iterator->state_ = RegistrationState::Failed;
                if (firstError.code == runtime::StatusCode::Ok)
                    firstError = { runtime::StatusCode::UnsafeTeardown, runtime::Subsystem::Hooks, 7, result.error().nativeCode };
                continue;
            }
            *iterator->spec_.originalSlot = nullptr;
            iterator->state_ = RegistrationState::Removed;
        }
        if (firstError.code != runtime::StatusCode::Ok)
            return runtime::Status::failure(firstError);
        registrations_.clear();
        return runtime::Status::success();
    }

    bool HookRegistry::hasActiveRegistrations() const noexcept
    {
        return std::any_of(registrations_.begin(), registrations_.end(), [](const HookRegistration& registration)
        {
            return registration.state_ == RegistrationState::Enabled || registration.state_ == RegistrationState::Failed;
        });
    }

    runtime::Status HookRegistry::rollbackCreated() noexcept
    {
        runtime::Error firstError{};
        for (auto iterator = registrations_.rbegin(); iterator != registrations_.rend(); ++iterator)
        {
            if (iterator->state_ != RegistrationState::Created)
                continue;
            const auto result = backend_.remove(iterator->spec_.target);
            if (!result)
            {
                iterator->state_ = RegistrationState::Failed;
                if (firstError.code == runtime::StatusCode::Ok)
                    firstError = { runtime::StatusCode::UnsafeTeardown, runtime::Subsystem::Hooks, 8, result.error().nativeCode };
                continue;
            }
            *iterator->spec_.originalSlot = nullptr;
            iterator->state_ = RegistrationState::Removed;
        }
        return firstError.code == runtime::StatusCode::Ok ? runtime::Status::success() : runtime::Status::failure(firstError);
    }
}
