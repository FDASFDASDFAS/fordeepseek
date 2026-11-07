#include "game_events.h"

#include "../../utils/memory/Interface/Interface.h"
#include "../../interfaces/interfaces.h"
#include "../../globals/globals.h"
#include "../feedback/hit_feedback.h"

#include <string>

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <unordered_map>

namespace tw::events
{
    namespace
    {
        // ------------------------------------------------------------------
        // CGameEventManager — client.dll, "GAMEEVENTSMANAGER002".
        //
        // Indices below were read off real call sites in client.dll rather
        // than assumed from an SDK header:
        //
        //   [3] AddListener(listener, name, serverSide)
        //       0x4EAE84: lea r8,[name] / xor r9d,r9d / mov rdx,listener
        //                 mov rax,[rcx] / call qword ptr [rax+0x18]
        //
        //   [5] RemoveListener(listener)
        //       ~30 sites, all of the shape
        //                 mov rax,[rcx] / mov rdx,listener
        //                 call qword ptr [rax+0x28]
        //       (only ever passed a listener pointer, behind a null check)
        // ------------------------------------------------------------------
        constexpr std::size_t kAddListener    = 3;
        constexpr std::size_t kRemoveListener = 5;

        // IGameEvent::GetName — vtable slot 1. Returns inline string storage:
        //   mov eax,[rcx+0x14] / bt eax,0x1e / jae .. / lea rax,[rcx+0x18]
        // Confirmed live: name sits at +0x18 with its length at +0x10.
        constexpr std::size_t kGetName = 1;

        // IGameEvent::GetInt — vtable slot 7.
        //   int GetInt(EventKey_t* key, int defaultValue)
        // Identified by return convention: the thunk (client.dll+0x99B510)
        // does `add rcx,0x58`, copies the 16-byte key to the stack and tail
        // calls 0x186F2D0, which returns `mov eax, edi` (edi = the default).
        // Slot 5 returns sete al (GetBool), 9 returns xmm0 (GetFloat),
        // 8 returns rax (GetUint64) — 7 is the 32-bit integer getter.
        //
        // Important: the helper looks the key up and returns the caller's
        // default when it isn't found (cmp r9d,-1 / je), so an unresolved key
        // degrades to the default instead of faulting.
        constexpr std::size_t kGetInt = 7;

        template <typename Fn>
        Fn vfunc(void* instance, std::size_t index) noexcept
        {
            return reinterpret_cast<Fn>((*reinterpret_cast<void***>(instance))[index]);
        }

        // The descriptor the getters take. Non-const on purpose: the engine
        // may memoise the resolved slot back into it.
        struct EventKey_t
        {
            std::uint32_t hash;
            std::uint32_t flags;
            const char*   name;
        };

        // Event keys are matched purely on this hash (client.dll+0x186D840
        // reads only key->hash, and even its linear miss path compares the
        // hash). It is MurmurHash2 seeded with 0x31415926 — the Source 2
        // string-token hash. Verified against the engine's own key table for
        // player_hurt: all 10 documented fields matched exactly.
        //
        // Note this is NOT the schema hash: schema uses fnv1a over
        // "Class->field". Different subsystem, different function.
        constexpr std::uint32_t murmur2(const char* key, std::uint32_t seed = 0x31415926u) noexcept
        {
            constexpr std::uint32_t m = 0x5BD1E995u;
            constexpr int r = 24;

            std::uint32_t length = 0;
            for (const char* scan = key; *scan; ++scan)
                ++length;

            std::uint32_t hash = seed ^ length;
            std::uint32_t remaining = length;
            const char* data = key;

            const auto byteAt = [](const char* p, int i) constexpr -> std::uint32_t {
                return static_cast<std::uint32_t>(static_cast<unsigned char>(p[i]));
            };

            while (remaining >= 4)
            {
                std::uint32_t block = byteAt(data, 0)
                    | (byteAt(data, 1) << 8)
                    | (byteAt(data, 2) << 16)
                    | (byteAt(data, 3) << 24);

                block *= m;
                block ^= block >> r;
                block *= m;

                hash *= m;
                hash ^= block;

                data += 4;
                remaining -= 4;
            }

            if (remaining == 3)
                hash ^= byteAt(data, 2) << 16;
            if (remaining >= 2)
                hash ^= byteAt(data, 1) << 8;
            if (remaining >= 1)
            {
                hash ^= byteAt(data, 0);
                hash *= m;
            }

            hash ^= hash >> 13;
            hash *= m;
            hash ^= hash >> 15;

            return hash;
        }

        int getInt(void* event, EventKey_t& key, int fallback) noexcept
        {
            if (!event)
                return fallback;
            return vfunc<int(__fastcall*)(void*, EventKey_t*, int)>(event, kGetInt)(event, &key, fallback);
        }


        void* g_manager = nullptr;
        bool  g_registered = false;

        const char* eventName(void* event) noexcept
        {
            if (!event)
                return nullptr;
            return vfunc<const char* (__fastcall*)(void*)>(event, kGetName)(event);
        }

        // event+0x08 -> descriptor, descriptor+0x0C -> numeric event id.
        // Taken from IGameEvent slot 2: mov rax,[rcx+8] / mov eax,[rax+0xC].
        // player_hurt should report id 16.
        int eventId(void* event) noexcept
        {
            if (!event)
                return -1;
            const auto descriptor = *reinterpret_cast<std::uintptr_t*>(
                reinterpret_cast<std::uintptr_t>(event) + 0x08);
            return descriptor ? *reinterpret_cast<int*>(descriptor + 0x0C) : -1;
        }



        constexpr int kMaxReports = 4;
        int g_reports = 0;

        class EventListener
        {
        public:
            virtual ~EventListener() = default;      // slot 0

            virtual void FireGameEvent(void* event)  // slot 1
            {
                const char* name = eventName(event);
                if (!name)
                    return;

                if (std::strcmp(name, "player_hurt") != 0)
                    return;

                static EventKey_t kUserId       { murmur2("userid"),     0, "userid" };
                static EventKey_t kAttacker     { murmur2("attacker"),   0, "attacker" };
                static EventKey_t kHealth       { murmur2("health"),     0, "health" };
                static EventKey_t kDamageHealth { murmur2("dmg_health"), 0, "dmg_health" };
                static EventKey_t kHitGroup     { murmur2("hitgroup"),   0, "hitgroup" };

                const int userId   = getInt(event, kUserId, -1);
                const int attacker = getInt(event, kAttacker, -1);
                const int damage   = getInt(event, kDamageHealth, 0);
                const int health   = getInt(event, kHealth, -1);
                const int hitGroup = getInt(event, kHitGroup, 0);

                // get_local_player() returns the controller entity index, which
                // is the player slot + 1; event fields carry the raw slot.
                const int localSlot = I::EngineClient
                    ? I::EngineClient->get_local_player() - 1
                    : -1;

                const bool byLocalPlayer = attacker >= 0 && attacker == localSlot;

                // Ignore self-damage so falling doesn't produce hit feedback.
                if (byLocalPlayer && userId != attacker)
                {
                    // Controllers are 1-based entity indices; event fields are slots.
                    std::string victimName = "Enemy";
                    entities::Vec3 victimPos{};
                    bool positionValid = false;

                    if (I::EntitySystem)
                    {
                        if (auto* controller =
                            I::EntitySystem->get_base_entity<CCSPlayerController>(userId + 1))
                        {
                            if (const char* named = controller->m_sSanitizedPlayerName())
                                victimName = named;

                            const CBaseHandle pawnHandle = controller->m_hPawn();
                            if (pawnHandle.valid() && I::GameEntity && I::GameEntity->Instance)
                            {
                                if (auto* pawn =
                                    I::GameEntity->Instance->Get<C_CSPlayerPawn>(pawnHandle))
                                {
                                    const Vector_t origin = pawn->m_vOldOrigin();
                                    victimPos = { origin.x, origin.y, origin.z };
                                    positionValid = true;
                                }
                            }
                        }
                    }

                    entities::Vec3 localEye{};
                    if (C_CSPlayerPawn* localPawn = g_ctx->local_pawn)
                    {
                        const Vector_t origin = localPawn->m_vOldOrigin();
                        const Vector_t offset = localPawn->m_vecViewOffset();
                        localEye = { origin.x + offset.x, origin.y + offset.y, origin.z + offset.z };
                    }

                    const double now = I::GlobalVars
                        ? static_cast<double>(I::GlobalVars->m_current_time)
                        : 0.0;

                    tw::features::g_hitFeedback.onLocalPlayerHit(
                        victimName, damage, health, hitGroup,
                        localEye, victimPos, positionValid, now);
                }

                if (g_reports < kMaxReports)
                {
                    ++g_reports;
                    char line[256]{};
                    std::snprintf(line, sizeof(line),
                        "[events] player_hurt  victim=%d attacker=%d local=%d  dmg=%d hp=%d  hitgroup=%d  %s",
                        userId, attacker, localSlot, damage, health, hitGroup,
                        byLocalPlayer ? "-> SOUND" : "(not us, ignored)");
                    std::cout << line << std::endl;

                    if (g_reports == kMaxReports)
                        std::cout << "[events] (log limit reached; sounds keep working)" << std::endl;
                }
            }
        };

        EventListener g_listener{};

        constexpr const char* kSubscribed[] = {
            "player_hurt",
            "bullet_impact",
            "round_start",
        };

        // CGameEventManager is not published through client.dll's
        // CreateInterface list (GAMEEVENTSMANAGER002 is present as a string but
        // never registered), so we take the same global client.dll itself loads
        // when it registers its own listeners.
        //
        // Rather than hardcode the offset, find every site shaped like
        //     mov rcx, [rip + disp32]      ; the manager
        //     xor r9d, r9d                 ; serverSide = false
        //     ... call/jmp qword ptr [rax+0x18]   ; vtable[3] == AddListener
        // and take the most-voted target. That survives codegen churn and
        // self-validates: a wrong global won't have dozens of AddListener
        // call sites pointing at it. On build 14172 this yields exactly one
        // candidate (client.dll+0x238D590) with 60 corroborating sites.
        void* resolveManagerByScan()
        {
            const auto base = reinterpret_cast<std::uint8_t*>(GetModuleHandleA("client.dll"));
            if (!base)
                return nullptr;

            const auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                return nullptr;
            const auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE)
                return nullptr;

            const std::size_t size = nt->OptionalHeader.SizeOfImage;
            std::unordered_map<std::uintptr_t, int> votes;

            for (std::size_t i = 0; i + 32 < size; ++i)
            {
                if (base[i] != 0x48 || base[i + 1] != 0x8B || base[i + 2] != 0x0D)
                    continue;                                   // mov rcx,[rip+d]
                if (base[i + 7] != 0x45 || base[i + 8] != 0x33 || base[i + 9] != 0xC9)
                    continue;                                   // xor r9d,r9d

                bool callsSlotThree = false;
                for (std::size_t j = i + 10; j + 2 < i + 26; ++j)
                {
                    // call/jmp qword ptr [rax+0x18]
                    if (base[j] == 0xFF && (base[j + 1] == 0x50 || base[j + 1] == 0x60)
                        && base[j + 2] == 0x18)
                    {
                        callsSlotThree = true;
                        break;
                    }
                }
                if (!callsSlotThree)
                    continue;

                const auto disp = *reinterpret_cast<const std::int32_t*>(base + i + 3);
                ++votes[reinterpret_cast<std::uintptr_t>(base + i + 7 + disp)];
            }

            std::uintptr_t best = 0;
            int bestVotes = 0;
            for (const auto& [address, count] : votes)
            {
                if (count > bestVotes)
                {
                    best = address;
                    bestVotes = count;
                }
            }

            if (!best || bestVotes < 4)
            {
                std::cout << "  events: [!] manager scan inconclusive (best votes="
                          << bestVotes << ")" << std::endl;
                return nullptr;
            }

            void* manager = *reinterpret_cast<void**>(best);

            // sanity: a live manager must have a vtable inside client.dll
            if (manager)
            {
                const auto vtable = *reinterpret_cast<std::uintptr_t*>(manager);
                const auto lo = reinterpret_cast<std::uintptr_t>(base);
                if (vtable < lo || vtable >= lo + size)
                {
                    std::cout << "  events: [!] manager vtable outside client.dll" << std::endl;
                    return nullptr;
                }
            }

            return manager;
        }
    }

    bool init()
    {
        g_manager = I::Get<void>("client.dll", "GAMEEVENTSMANAGER002");
        if (!g_manager)
            g_manager = resolveManagerByScan();

        if (!g_manager)
        {
            std::cout << "  events: [!] could not resolve CGameEventManager" << std::endl;
            return false;
        }

        const auto addListener =
            vfunc<bool(__fastcall*)(void*, void*, const char*, bool)>(g_manager, kAddListener);

        int registered = 0;
        for (const char* name : kSubscribed)
        {
            if (addListener(g_manager, &g_listener, name, false))
                ++registered;
            else
                std::cout << "  events: [!] failed to subscribe " << name << std::endl;
        }

        g_registered = registered > 0;

        std::cout << "  events: manager=" << g_manager
                  << " subscribed " << registered << "/"
                  << static_cast<int>(std::size(kSubscribed)) << std::endl;

        return g_registered;
    }

    void shutdown()
    {
        if (!g_manager || !g_registered)
        {
            g_manager = nullptr;
            return;
        }

        vfunc<void(__fastcall*)(void*, void*)>(g_manager, kRemoveListener)(g_manager, &g_listener);

        g_registered = false;
        g_manager = nullptr;
    }
}
