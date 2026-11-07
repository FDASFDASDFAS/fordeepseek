#pragma once

#include "settings.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tw::config
{
    [[nodiscard]] bool validProfileName(std::string_view name) noexcept;
    [[nodiscard]] runtime::Result<Settings> decodeSettings(std::string_view jsonText) noexcept;
    [[nodiscard]] runtime::Result<std::string> encodeSettings(const Settings& settings) noexcept;

    class SettingsStore final
    {
    public:
        explicit SettingsStore(std::filesystem::path directory) : directory_(std::move(directory)) {}

        [[nodiscard]] runtime::Result<Settings> load(std::string_view profileName) const noexcept;
        [[nodiscard]] runtime::Status save(std::string_view profileName, const Settings& settings) const noexcept;
        [[nodiscard]] runtime::Status remove(std::string_view profileName) const noexcept;
        [[nodiscard]] std::vector<std::string> list() const noexcept;
        [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }

    private:
        [[nodiscard]] std::filesystem::path pathFor(std::string_view profileName) const;
        std::filesystem::path directory_{};
    };
}
