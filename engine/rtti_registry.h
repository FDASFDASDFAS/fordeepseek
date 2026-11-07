#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tw::engine
{
    struct VtableEvidence final
    {
        std::uintptr_t address{};
        std::size_t executableEntries{};
        std::vector<std::uintptr_t> functions{};
    };

    struct RttiTypeEvidence final
    {
        bool typeDescriptorFound{};
        std::vector<VtableEvidence> vtables{};
    };

    // Inventories standard MSVC x64 RTTI in one loaded PE image; it does not identify method semantics.
    [[nodiscard]] RttiTypeEvidence findMsvcVtables(HMODULE module, std::string_view decoratedTypeName) noexcept;
}
