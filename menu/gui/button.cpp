#include "shell.h"
#include "styling.h"
#include "fonts.h"

#include <algorithm>

namespace ksd
{
	bool Button(const char* label, const ImVec2& size) noexcept
	{
		const ImVec2 labelSize = fonts::CalcUiTextSize(label);
		const float w = size.x > 0.f ? size.x : ContentWidth();
		const float h = size.y > 0.f ? size.y : 25.f;
		const ImVec2 drawPos = CursorPos() + ImVec2(20, 0);
		const ImRect bb(drawPos, drawPos + ImVec2(w, h));

		const bool hovered = bb.Contains(ImGui::GetMousePos());
		const bool held = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		const bool pressed = ButtonBehavior(label, bb, true);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		if (held)
			style::drawGradientV(dl, bb.Min, bb.Max, style::glass(28, 28, 28, style::glassWidget), style::glass(33, 33, 33, style::glassWidget));
		else if (hovered)
			style::drawGradientV(dl, bb.Min, bb.Max, style::glass(45, 45, 45, style::glassWidget), style::glass(38, 38, 38, style::glassWidget));
		else
			style::drawGradientV(dl, bb.Min, bb.Max, style::glass(35, 35, 35, style::glassWidget), style::glass(30, 30, 30, style::glassWidget));

		dl->AddRect(bb.Min, bb.Max, style::col::widgetBorder());
		dl->AddRect(bb.Min + ImVec2(1, 1), bb.Max - ImVec2(1, 1), style::col::border());

		const ImVec2 textPos(
			bb.Min.x + (bb.GetWidth() - labelSize.x) * 0.5f,
			bb.Min.y + (bb.GetHeight() - labelSize.y) * 0.5f);
		const ImVec2 textMax = bb.Max - ImVec2(5, 0);
		const float textX = std::clamp(textPos.x + 1.f, bb.Min.x + 3.f, textMax.x - labelSize.x);
		dl->AddText(fonts::Verdana(), fonts::kUiSize, ImVec2(textX, textPos.y + 1), style::col::textShadow(), label);
		dl->AddText(fonts::Verdana(), fonts::kUiSize, ImVec2(textX - 1.f, textPos.y), style::col::text(), label);

		AddItem(ImVec2(w + 20.f, h + 5.f));
		return pressed;
	}
}
