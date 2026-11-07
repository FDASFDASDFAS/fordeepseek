#include "rtti_registry.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace tw::engine
{
    namespace
    {
        struct SectionView final
        {
            const std::uint8_t* begin{};
            std::size_t size{};
            bool executable{};
        };

        struct ImageView final
        {
            const std::uint8_t* base{};
            std::size_t size{};
            std::vector<SectionView> sections{};

            [[nodiscard]] bool contains(const void* pointer, const std::size_t length) const noexcept
            {
                const auto address = reinterpret_cast<std::uintptr_t>(pointer);
                const auto start = reinterpret_cast<std::uintptr_t>(base);
                return address >= start && length <= size && address - start <= size - length;
            }

            [[nodiscard]] bool isExecutable(const std::uintptr_t address) const noexcept
            {
                return std::any_of(sections.begin(), sections.end(), [address](const SectionView& section)
                {
                    const auto begin = reinterpret_cast<std::uintptr_t>(section.begin);
                    return section.executable && address >= begin && address - begin < section.size;
                });
            }
        };

        template <typename T>
        T read(const std::uint8_t* address) noexcept
        {
            T value{};
            std::memcpy(&value, address, sizeof(value));
            return value;
        }

        ImageView inspectImage(const HMODULE module) noexcept
        {
            ImageView image{};
            if (!module)
                return image;
            const auto* base = reinterpret_cast<const std::uint8_t*>(module);
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
                return image;
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt->OptionalHeader.SizeOfImage < nt->OptionalHeader.SizeOfHeaders || nt->FileHeader.NumberOfSections > 96)
                return image;

            image.base = base;
            image.size = nt->OptionalHeader.SizeOfImage;
            const IMAGE_SECTION_HEADER* headers = IMAGE_FIRST_SECTION(nt);
            for (std::uint16_t index = 0; index < nt->FileHeader.NumberOfSections; ++index)
            {
                const auto& header = headers[index];
                const std::size_t virtualAddress = header.VirtualAddress;
                const std::size_t virtualSize = header.Misc.VirtualSize;
                if (virtualAddress >= image.size || virtualSize == 0)
                    continue;
                image.sections.push_back({ base + virtualAddress, (std::min)(virtualSize, image.size - virtualAddress),
                    (header.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 });
            }
            return image;
        }

        std::vector<std::uintptr_t> collectExecutableEntries(const ImageView& image, const std::uint8_t* vtable)
        {
            constexpr std::size_t maximumEntries = 128;
            std::vector<std::uintptr_t> functions;
            while (functions.size() < maximumEntries &&
                image.contains(vtable + functions.size() * sizeof(std::uintptr_t), sizeof(std::uintptr_t)))
            {
                const auto function = read<std::uintptr_t>(vtable + functions.size() * sizeof(std::uintptr_t));
                if (!image.isExecutable(function))
                    break;
                functions.push_back(function);
            }
            return functions;
        }
    }

    RttiTypeEvidence findMsvcVtables(const HMODULE module, const std::string_view decoratedTypeName) noexcept
    {
        RttiTypeEvidence evidence{};
        if (decoratedTypeName.empty())
            return evidence;
        const ImageView image = inspectImage(module);
        if (!image.base || image.size > (std::numeric_limits<std::uint32_t>::max)())
            return evidence;

        std::vector<std::uint32_t> typeDescriptorRvas;
        for (const auto& section : image.sections)
        {
            if (section.executable || section.size <= decoratedTypeName.size() + 16)
                continue;
            const auto end = section.begin + section.size;
            for (const auto* cursor = section.begin + 16; cursor + decoratedTypeName.size() < end; ++cursor)
            {
                if (std::memcmp(cursor, decoratedTypeName.data(), decoratedTypeName.size()) != 0 ||
                    cursor[decoratedTypeName.size()] != '\0')
                    continue;
                const auto* descriptor = cursor - 16;
                const auto rva = static_cast<std::uint32_t>(descriptor - image.base);
                typeDescriptorRvas.push_back(rva);
                evidence.typeDescriptorFound = true;
            }
        }

        std::vector<const std::uint8_t*> locators;
        for (const std::uint32_t typeRva : typeDescriptorRvas)
        {
            for (const auto& section : image.sections)
            {
                if (section.executable || section.size < 24)
                    continue;
                for (std::size_t offset = 0; offset + 24 <= section.size; offset += alignof(std::uint32_t))
                {
                    const auto* candidate = section.begin + offset;
                    if (read<std::uint32_t>(candidate) != 1 || read<std::uint32_t>(candidate + 12) != typeRva)
                        continue;
                    const auto selfRva = read<std::uint32_t>(candidate + 20);
                    if (selfRva < image.size && image.base + selfRva == candidate)
                        locators.push_back(candidate);
                }
            }
        }

        for (const auto* locator : locators)
        {
            const auto locatorAddress = reinterpret_cast<std::uintptr_t>(locator);
            for (const auto& section : image.sections)
            {
                if (section.executable || section.size < sizeof(std::uintptr_t) * 2)
                    continue;
                for (std::size_t offset = 0; offset + sizeof(std::uintptr_t) * 2 <= section.size; offset += sizeof(std::uintptr_t))
                {
                    const auto* reference = section.begin + offset;
                    if (read<std::uintptr_t>(reference) != locatorAddress)
                        continue;
                    const auto* vtable = reference + sizeof(std::uintptr_t);
                    auto functions = collectExecutableEntries(image, vtable);
                    if (functions.empty())
                        continue;
                    const auto address = reinterpret_cast<std::uintptr_t>(vtable);
                    const bool duplicate = std::any_of(evidence.vtables.begin(), evidence.vtables.end(), [address](const VtableEvidence& item)
                    {
                        return item.address == address;
                    });
                    if (!duplicate)
                        evidence.vtables.push_back({ address, functions.size(), std::move(functions) });
                }
            }
        }
        return evidence;
    }
}
