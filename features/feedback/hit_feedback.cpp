#include "hit_feedback.h"

#include "../../config/configmanager.h"
#include "../../utils/convar.h"
#include "../../utils/math/viewmatrix/viewmatrix.h"

#include <Windows.h>

#include "../../utils/memory/patternscan/patternscan.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

namespace tw::features
{
    HitFeedback g_hitFeedback{};

    namespace
    {
        bool projectPoint(const viewmatrix_t& matrix, const entities::Vec3& pos, float width, float height, renderer::Point& out)
        {
            const float w = matrix.matrix[3][0] * pos.x + matrix.matrix[3][1] * pos.y +
                matrix.matrix[3][2] * pos.z + matrix.matrix[3][3];
            if (!std::isfinite(w) || w <= 0.001f || width <= 0.0f || height <= 0.0f)
                return false;
            const float inverse = 1.0f / w;
            const float centerX = width * 0.5f;
            const float centerY = height * 0.5f;
            out.x = centerX + ((matrix.matrix[0][0] * pos.x + matrix.matrix[0][1] * pos.y +
                matrix.matrix[0][2] * pos.z + matrix.matrix[0][3]) * inverse * centerX);
            out.y = centerY - ((matrix.matrix[1][0] * pos.x + matrix.matrix[1][1] * pos.y +
                matrix.matrix[1][2] * pos.z + matrix.matrix[1][3]) * inverse * centerY);
            return std::isfinite(out.x) && std::isfinite(out.y);
        }

        renderer::Color toColor(const config::Color4& src, float alphaMultiplier = 1.0f)
        {
            return { src.r, src.g, src.b, std::clamp(src.a * alphaMultiplier, 0.0f, 1.0f) };
        }

        // ------------------------------------------------------------------
        // Engine sound backend.
        //
        // soundsystem.dll exports (by pattern) PlayVSnd(const char* resource),
        // which takes a full compiled resource path — "sounds/<dir>/<name>.vsnd_c",
        // extension included. It has no volume parameter: the engine scales
        // tool-played sounds by snd_toolvolume, so we write that convar
        // directly before each call.
        // ------------------------------------------------------------------

        // The folder we drop our .vsnd_c files into, both on disk
        // (<game>\csgo\sounds\<dir>\) and in the resource path (sounds/<dir>/).
        constexpr const char* kSoundFolder = "templeware";

        using PlayVSnd_t = void(__fastcall*)(const char*);
        using ConVarValue_t = float* (__fastcall*)(void*, int);

        PlayVSnd_t g_playVSnd = nullptr;
        ConVarValue_t g_conVarValue = nullptr;
        void* g_sndToolVolume = nullptr;
        std::string g_soundDir;

        void setToolVolume(float volume)
        {
            if (!g_sndToolVolume)
                return;

            volume = std::clamp(volume, 0.0f, 1.0f);

            // ConVar::GetValuePtr(this, splitScreenSlot) — slot -1 means "the
            // global value", which is what the engine itself reads.
            if (g_conVarValue)
            {
                if (float* value = g_conVarValue(g_sndToolVolume, -1))
                {
                    *value = volume;
                    return;
                }
            }

            // fallback: ConVar -> ConVarData* at +0x08, value storage at +0x58
            const auto data = *reinterpret_cast<std::uintptr_t*>(
                reinterpret_cast<std::uintptr_t>(g_sndToolVolume) + 0x08);
            if (data)
                *reinterpret_cast<float*>(data + 0x58) = volume;
        }

        // Where the pack lives when nothing is found next to the dll — i.e.
        // when the dll is injected from a copy somewhere else.
        constexpr const char* kFallbackSourceDir =
            "C:\\Users\\vrdgo\\OneDrive\\Desktop\\TempleWare-CS2-1.1.5\\hitsounds-master\\";

        bool isDirectory(const std::string& path)
        {
            const DWORD attributes = GetFileAttributesA(path.c_str());
            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
        }

        // Locates the sound pack by walking up from wherever the dll was
        // loaded, so this works from a build output dir or from next to the
        // dll once deployed.
        bool findSourceDir(std::string& out)
        {
            HMODULE self = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&findSourceDir), &self))
            {
                char modulePath[MAX_PATH]{};
                if (GetModuleFileNameA(self, modulePath, MAX_PATH))
                {
                    std::cout << "  hitsounds: dll loaded from " << modulePath << std::endl;

                    std::string dir(modulePath);
                    const auto slash = dir.rfind('\\');
                    if (slash != std::string::npos)
                    {
                        dir.resize(slash);

                        for (int level = 0; level < 6; ++level)
                        {
                            const std::string candidate = dir + "\\hitsounds-master\\";
                            if (isDirectory(candidate))
                            {
                                out = candidate;
                                return true;
                            }

                            const auto up = dir.rfind('\\');
                            if (up == std::string::npos)
                                break;
                            dir.resize(up);
                        }
                    }
                }
            }

            if (isDirectory(kFallbackSourceDir))
            {
                out = kFallbackSourceDir;
                return true;
            }

            return false;
        }

        // Copies the pack into <game>\csgo\sounds\<kSoundFolder>\. Note "sounds"
        // (plural) — that is the resource root CS2 mounts, and csgo\sound\ does
        // not exist, so CreateDirectory there fails and every copy fails with it.
        void copyHitSoundsToGameDir()
        {
            char exePath[MAX_PATH]{};
            if (!GetModuleFileNameA(GetModuleHandleA(nullptr), exePath, MAX_PATH))
            {
                std::cout << "  hitsounds: failed to resolve game path" << std::endl;
                return;
            }

            std::cout << "  hitsounds: game exe " << exePath << std::endl;

            // <game>\bin\win64\cs2.exe -> <game> : strip cs2.exe, win64, bin.
            // Done by trimming components rather than appending "..\..\" and
            // normalising, so the path we build is the path we use.
            std::string gameDir(exePath);
            for (int i = 0; i < 3; ++i)
            {
                const auto slash = gameDir.rfind('\\');
                if (slash == std::string::npos)
                {
                    std::cout << "  hitsounds: [!] unexpected exe path layout" << std::endl;
                    return;
                }
                gameDir.resize(slash);
            }

            std::string soundsRoot = gameDir;
            soundsRoot += "\\csgo\\sounds\\";
            CreateDirectoryA(soundsRoot.c_str(), nullptr);

            g_soundDir = soundsRoot + kSoundFolder + "\\";
            if (!CreateDirectoryA(g_soundDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            {
                std::cout << "  hitsounds: [!] cannot create " << g_soundDir
                          << " (error " << GetLastError() << ")" << std::endl;
                g_soundDir.clear();
                return;
            }

            std::string srcDir;
            if (!findSourceDir(srcDir))
            {
                std::cout << "  hitsounds: [!] hitsounds-master not found" << std::endl;
                g_soundDir.clear();
                return;
            }

            int copied = 0;
            std::string firstError;
            for (int i = 0; i < HitFeedback::kHitSoundCount; ++i)
            {
                const std::string src = srcDir + HitFeedback::kHitSoundNames[i] + ".vsnd_c";
                const std::string dst = g_soundDir + HitFeedback::kHitSoundNames[i] + ".vsnd_c";
                if (CopyFileA(src.c_str(), dst.c_str(), FALSE))
                    ++copied;
                else if (firstError.empty())
                    firstError = src + " (error " + std::to_string(GetLastError()) + ")";
            }

            std::cout << "  hitsounds: copied " << copied << "/" << HitFeedback::kHitSoundCount
                      << " from " << srcDir << " to " << g_soundDir << std::endl;
            if (!firstError.empty())
                std::cout << "  hitsounds: [!] first failure: " << firstError << std::endl;

            if (copied == 0)
                g_soundDir.clear();
        }

        void playHitSound(int index, float volume)
        {
            if (!g_playVSnd || g_soundDir.empty())
                return;
            if (index < 0 || index >= HitFeedback::kHitSoundCount)
                return;

            setToolVolume(volume);

            std::string resource = "sounds/";
            resource += kSoundFolder;
            resource += '/';
            resource += HitFeedback::kHitSoundNames[index];
            resource += ".vsnd_c";

            g_playVSnd(resource.c_str());
        }
    }

    void HitFeedback::initHitSounds()
    {
        // PlayVSnd(const char* pszResource)
        g_playVSnd = reinterpret_cast<PlayVSnd_t>(M::patternScan("soundsystem",
            "48 89 5C 24 08 48 89 74 24 18 48 89 7C 24 20 55 48 8D 6C 24 80 48 81 EC"));

        // ConVar::GetValuePtr(this, splitScreenSlot)
        g_conVarValue = reinterpret_cast<ConVarValue_t>(M::patternScan("soundsystem",
            "4C 8B 49 08 49 8B 49 30 4D 8D 41 58 48 0F BA E1 0F"));

        g_sndToolVolume = tw::convars::findRegistered("soundsystem.dll", "snd_toolvolume");

        std::cout << "  hitsounds: PlayVSnd=" << reinterpret_cast<void*>(g_playVSnd)
                  << " GetValuePtr=" << reinterpret_cast<void*>(g_conVarValue)
                  << " snd_toolvolume=" << g_sndToolVolume << std::endl;

        if (!g_playVSnd)
            std::cout << "  hitsounds: [!] PlayVSnd not resolved - is soundsystem.dll loaded?" << std::endl;

        copyHitSoundsToGameDir();
    }

    void HitFeedback::onLocalPlayerHit(const std::string& victimName, int damage, int remainingHealth,
        int hitgroup, const entities::Vec3& localEye, const entities::Vec3& victimPos,
        bool positionValid, double nowSeconds)
    {
        const auto settings = internal_config::ConfigManager::Snapshot();
        if (!settings)
            return;

        const bool isKill = remainingHealth <= 0;

        hits_.push_back({ nowSeconds, victimName, damage, remainingHealth, isKill, victimPos });
        while (hits_.size() > 10)
            hits_.pop_front();

        lastHitTime_ = nowSeconds;
        lastHitWasKill_ = isKill;

        // Tracer/impact need a world position; skip them if the victim's pawn
        // could not be resolved rather than drawing at the origin.
        if (positionValid)
        {
            const entities::Vec3 chestTarget{ victimPos.x, victimPos.y, victimPos.z + 36.0f };

            tracers_.push_back({ nowSeconds, localEye, chestTarget, true });
            while (tracers_.size() > 20)
                tracers_.pop_front();

            impacts_.push_back({ nowSeconds, chestTarget, true });
            while (impacts_.size() > 30)
                impacts_.pop_front();
        }

        if (settings->feedback.hitSound && settings->feedback.soundVolume > 0.01f)
        {
            const bool headshot = hitgroup == 1;
            const int index = (headshot && settings->feedback.separateHeadshotSound)
                ? settings->feedback.headshotSoundType
                : settings->feedback.soundType;
            playHitSound(index, settings->feedback.soundVolume);
        }
    }

    void HitFeedback::onEnemyDamaged(const std::string& name, int damage, int remainingHealth,
        const entities::Vec3& localEye, const entities::Vec3& enemyPos, double nowSeconds)
    {
        const bool isKill = remainingHealth <= 0;
        hits_.push_back({ nowSeconds, name, damage, remainingHealth, isKill, enemyPos });
        while (hits_.size() > 10)
            hits_.pop_front();

        const entities::Vec3 chestTarget{ enemyPos.x, enemyPos.y, enemyPos.z + 36.0f };

        tracers_.push_back({ nowSeconds, localEye, chestTarget, true });
        while (tracers_.size() > 20)
            tracers_.pop_front();

        impacts_.push_back({ nowSeconds, chestTarget, true });
        while (impacts_.size() > 30)
            impacts_.pop_front();

        lastHitTime_ = nowSeconds;
        lastHitWasKill_ = isKill;

        const auto settings = internal_config::ConfigManager::Snapshot();
        // Sound is intentionally not triggered here — a health drop says nothing
        // about who caused it. tw::events drives it from player_hurt instead.
        (void)settings;
    }

    void HitFeedback::onShotFired(const entities::Vec3& localEye, const entities::Vec3& viewForward, double nowSeconds)
    {
        const entities::Vec3 endPos{
            localEye.x + viewForward.x * 3000.0f,
            localEye.y + viewForward.y * 3000.0f,
            localEye.z + viewForward.z * 3000.0f
        };

        tracers_.push_back({ nowSeconds, localEye, endPos, false });
        while (tracers_.size() > 20)
            tracers_.pop_front();

        impacts_.push_back({ nowSeconds, endPos, false });
        while (impacts_.size() > 30)
            impacts_.pop_front();
    }

    void HitFeedback::clear() noexcept
    {
        hits_.clear();
        tracers_.clear();
        impacts_.clear();
        lastHitTime_ = -10.0;
        lastHitWasKill_ = false;
    }

    void HitFeedback::renderOverlay(std::vector<renderer::DrawCommand>& commands,
        const viewmatrix_t& viewMatrix, float screenWidth, float screenHeight, double nowSeconds)
    {
        const auto settings = internal_config::ConfigManager::Snapshot();
        if (!settings)
            return;

        const auto& cfg = settings->feedback;

        if (cfg.hitMarker && lastHitTime_ > 0.0)
        {
            const double elapsed = nowSeconds - lastHitTime_;
            constexpr double markerDuration = 0.45;
            if (elapsed >= 0.0 && elapsed < markerDuration)
            {
                const float alpha = static_cast<float>(1.0 - (elapsed / markerDuration));
                const float cx = screenWidth * 0.5f;
                const float cy = screenHeight * 0.5f;
                constexpr float gap = 4.0f;
                constexpr float length = 6.0f;

                const renderer::Color markerColor = lastHitWasKill_
                    ? renderer::Color{ 1.0f, 0.2f, 0.2f, alpha }
                    : renderer::Color{ 1.0f, 1.0f, 1.0f, alpha };

                commands.emplace_back(renderer::Line{ { cx - gap - length, cy - gap - length }, { cx - gap, cy - gap }, markerColor, 1.5f });
                commands.emplace_back(renderer::Line{ { cx + gap, cy - gap }, { cx + gap + length, cy - gap - length }, markerColor, 1.5f });
                commands.emplace_back(renderer::Line{ { cx - gap - length, cy + gap + length }, { cx - gap, cy + gap }, markerColor, 1.5f });
                commands.emplace_back(renderer::Line{ { cx + gap, cy + gap }, { cx + gap + length, cy + gap + length }, markerColor, 1.5f });
            }
        }

        if (cfg.hitLogs && !hits_.empty())
        {
            constexpr double logDuration = 3.5;
            float logY = 20.0f;

            for (const auto& hit : hits_)
            {
                const double elapsed = nowSeconds - hit.timeSeconds;
                if (elapsed < 0.0 || elapsed >= logDuration)
                    continue;

                const float alpha = std::clamp(static_cast<float>(1.0 - (elapsed / logDuration)), 0.0f, 1.0f);
                char buffer[128]{};
                if (hit.isKill)
                    std::snprintf(buffer, sizeof(buffer), "[TW] KILLED %s (-%d HP)", hit.targetName.c_str(), hit.damage);
                else
                    std::snprintf(buffer, sizeof(buffer), "[TW] Hit %s for %d damage (%d HP remaining)",
                        hit.targetName.c_str(), hit.damage, hit.remainingHealth);

                const renderer::Color textColor = hit.isKill
                    ? renderer::Color{ 1.0f, 0.3f, 0.3f, alpha }
                    : renderer::Color{ 0.4f, 0.9f, 1.0f, alpha };

                const renderer::Color shadowColor{ 0.0f, 0.0f, 0.0f, alpha * 0.8f };
                commands.emplace_back(renderer::Text{ { 21.0f, logY + 1.0f }, shadowColor, buffer, false });
                commands.emplace_back(renderer::Text{ { 20.0f, logY }, textColor, buffer, false });
                logY += 16.0f;
            }
        }

        if (cfg.bulletTracers && !tracers_.empty())
        {
            constexpr double tracerDuration = 1.2;
            for (const auto& tracer : tracers_)
            {
                const double elapsed = nowSeconds - tracer.timeSeconds;
                if (elapsed < 0.0 || elapsed >= tracerDuration)
                    continue;

                const float alpha = std::clamp(static_cast<float>(1.0 - (elapsed / tracerDuration)), 0.0f, 1.0f);
                renderer::Point startScreen{};
                renderer::Point endScreen{};

                if (projectPoint(viewMatrix, tracer.startPosition, screenWidth, screenHeight, startScreen) &&
                    projectPoint(viewMatrix, tracer.endPosition, screenWidth, screenHeight, endScreen))
                {
                    const renderer::Color tracerColor = toColor(cfg.tracerColor, alpha);
                    commands.emplace_back(renderer::Line{ startScreen, endScreen, tracerColor, 1.5f });
                }
            }
        }

        if (cfg.bulletImpacts && !impacts_.empty())
        {
            constexpr double impactDuration = 1.5;
            for (const auto& impact : impacts_)
            {
                const double elapsed = nowSeconds - impact.timeSeconds;
                if (elapsed < 0.0 || elapsed >= impactDuration)
                    continue;

                const float alpha = std::clamp(static_cast<float>(1.0 - (elapsed / impactDuration)), 0.0f, 1.0f);
                renderer::Point screenPoint{};

                if (projectPoint(viewMatrix, impact.position, screenWidth, screenHeight, screenPoint))
                {
                    const renderer::Color impactColor = toColor(cfg.impactColor, alpha);
                    commands.emplace_back(renderer::Rectangle{
                        { screenPoint.x - 3.0f, screenPoint.y - 3.0f },
                        { screenPoint.x + 3.0f, screenPoint.y + 3.0f },
                        impactColor, 1.5f, false });
                    commands.emplace_back(renderer::Rectangle{
                        { screenPoint.x - 2.0f, screenPoint.y - 2.0f },
                        { screenPoint.x + 2.0f, screenPoint.y + 2.0f },
                        impactColor, 1.0f, true });
                }
            }
        }
    }
}
