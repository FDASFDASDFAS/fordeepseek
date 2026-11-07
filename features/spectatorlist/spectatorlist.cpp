#include "spectatorlist.h"
#include "../../../cs2/entity/C_EntitySystem/entity.h"

#include <mutex>
#include <string>
#include <vector>

namespace
{
	std::mutex spectatorMutex;
	std::vector<std::string> spectatorNames;
	bool localAvailable{};
}

void SpectatorList::Capture()
{
	std::vector<std::string> captured;
	bool available = false;

	if (I::EngineClient->connected() && I::EngineClient->in_game())
	{
		C_CSPlayerPawn* localPawn = g_ctx->local_pawn;
		if (localPawn && localPawn->m_iHealth() > 0)
		{
			CCSPlayerController* localController = I::GameEntity->Instance->Get<CCSPlayerController>(localPawn->m_hController());
			if (localController && localController->m_bPawnIsAlive() && localController->m_hPawn().valid())
			{
				available = true;
				for (CEntityInstance* entity : g_entitySystem->get("CCSPlayerController"))
				{
					if (!entity || captured.size() >= 64)
						continue;
					auto* controller = reinterpret_cast<CCSPlayerController*>(entity);
					if (!controller->m_hObserverPawn().valid() || controller->m_bPawnIsAlive())
						continue;
					auto* observerPawn = I::GameEntity->Instance->Get<C_CSPlayerPawn>(controller->m_hObserverPawn());
					if (!observerPawn)
						continue;
					auto* observerServices = observerPawn->m_pObserverServices();
					if (!observerServices || observerServices->m_hObserverTarget() != localController->m_hPawn())
						continue;
					const char* name = controller->m_sSanitizedPlayerName();
					captured.emplace_back(name ? name : "");
				}
			}
		}
	}

	std::scoped_lock lock{ spectatorMutex };
	localAvailable = available;
	spectatorNames = std::move(captured);
}

void SpectatorList::Clear()
{
	std::scoped_lock lock{ spectatorMutex };
	localAvailable = false;
	spectatorNames.clear();
}

void SpectatorList::OnRender()
{
	if (!Config::visual_world.spectatorList)
		return;

	std::vector<std::string> names;
	{
		std::scoped_lock lock{ spectatorMutex };
		if (!localAvailable)
			return;
		names = spectatorNames;
	}

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.13f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.30f, 0.30f, 0.7f));
	ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(1700, 100), ImGuiCond_FirstUseEver);
	ImGui::Begin("Spectator List", nullptr,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoScrollbar);
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "SPECTATORS:");
	for (const auto& name : names)
		ImGui::Text("%s", name.c_str());
	ImGui::End();
	ImGui::PopStyleColor(3);
}
