#include "shell.h"
#include "styling.h"
#include "fonts.h"

#include <cstdio>
#include <string>

namespace ksd
{
	static bool DrawCheckbox(const char* label, bool* v, bool special, int* keybind) noexcept
	{
		const float  itemHeight = 14.f;
		const ImVec2 drawPos    = ksd::CursorPos();
		const ImRect totalBb(drawPos, drawPos + ImVec2(150.f, itemHeight));

		const bool hovered = totalBb.Contains(ImGui::GetMousePos());
		if (ksd::PopUpsAreClosed() && hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
			&& ksd::SelectedItem()[0] == '\0')
		{
			*v = !*v;
		}

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float  checkboxY = drawPos.y + (itemHeight - 8.f) * 0.5f;
		const ImVec2 boxMin(drawPos.x + 1, checkboxY + 1);
		const ImVec2 boxInner(6, 6);

		if (*v)
		{
			dl->AddRectFilled(boxMin, boxMin + boxInner, style::accentU32());
		}
		else if (hovered)
		{
			style::drawGradientV(dl, boxMin, boxMin + boxInner, style::col::chkHoverTop(), style::col::chkHoverBot());
		}
		else
		{
			style::drawGradientV(dl, boxMin, boxMin + boxInner, style::col::chkIdleTop(), style::col::chkIdleBot());
		}

		dl->AddRect(ImVec2(drawPos.x, checkboxY), ImVec2(drawPos.x + 8, checkboxY + 8), style::col::widgetBorder());

		const float  textY   = drawPos.y + (itemHeight - 12.f) * 0.5f;
		const ImU32  textCol = special ? style::col::textSpecial() : style::col::text();
		dl->AddText(fonts::Verdana(), fonts::kUiSize, ImVec2(drawPos.x + 20, textY), textCol, label);

		ksd::AddItem(ImVec2(150.f, itemHeight));

		if (keybind != nullptr)
		{
			char kbId[24];
			std::snprintf(kbId, sizeof(kbId), "##kb_%p", static_cast<void*>(keybind));
			ksd::DrawKeybindButton(kbId, keybind, v, drawPos.y);
		}

		return *v;
	}

	bool Checkbox(const char* label, bool* v, bool special) noexcept
	{
		return DrawCheckbox(label, v, special, nullptr);
	}

	bool CheckboxKeybind(const char* label, bool* v, int* keybind, bool special) noexcept
	{
		return DrawCheckbox(label, v, special, keybind);
	}
}
