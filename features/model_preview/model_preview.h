#pragma once

#include <Windows.h>
#include <d3d11.h>

#include "../../../../external/imgui/imgui.h"

namespace tw::model_preview
{
	enum class Status : unsigned char
	{
		Unavailable,
		AwaitingPanorama,
		AwaitingTexture,
		Ready
	};

	// Optional feature: unresolved current-image contracts disable only the preview.
	bool init() noexcept;
	bool disableHooks() noexcept;
	bool shutdown() noexcept;

	void requestCreation() noexcept;
	void requestCleanup() noexcept;
	bool cleanupComplete() noexcept;
	bool waitForCleanup(DWORD timeoutMs) noexcept;

	Status status() noexcept;
	const char* statusText() noexcept;
	void onGeneratePrimitives() noexcept;
	bool isPreviewSceneObject(void* sceneObject) noexcept;
	void draw(const ImVec2& min, const ImVec2& max) noexcept;
}
