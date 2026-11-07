#pragma once
#include <memory>
#include "../../interfaces/CUserCmd/CUserCmd.h"
#include "../../hooks/hooks.h"
#include "../../interfaces/interfaces.h"
#include "../../config/config.h"

class AntiAim {
private:
	bool m_should_invert{};
	void SetYaw(CUserCmd* user_cmd);
	void SetPitch(CUserCmd* user_cmd);

public:
	void OnCreateMove(CUserCmd* user_cmd);
};

const auto g_antiaim = std::make_unique<AntiAim>();