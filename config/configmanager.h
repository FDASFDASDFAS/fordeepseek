#pragma once

#include "settings.h"

#include <memory>
#include <string>
#include <vector>

namespace internal_config
{
    class ConfigManager final
    {
    public:
        [[nodiscard]] static std::vector<std::string> ListConfigs();
        [[nodiscard]] static bool Save(const std::string& configName) noexcept;
        [[nodiscard]] static bool Load(const std::string& configName) noexcept;
        [[nodiscard]] static bool Remove(const std::string& configName) noexcept;
        [[nodiscard]] static bool PublishCurrent() noexcept;
        [[nodiscard]] static std::shared_ptr<const tw::config::Settings> Snapshot() noexcept;
    };
}
