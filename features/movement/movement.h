#pragma once
#include <memory>
#include "../../interfaces/CUserCmd/CUserCmd.h"
#include "../../hooks/hooks.h"
#include "../../interfaces/interfaces.h"
#include "../../config/config.h"

class Movement {
private:
	void Bhop(CUserCmd* user_cmd);
	void AutoStrafe(CUserCmd* user_cmd, Vector_t view_angle);
public:
	void OnCreateMove(CUserCmd* user_cmd, Vector_t viewAngle);
	void MovementFix(CUserCmd* user_cmd, Vector_t viewAngle);
};

const auto g_movement = std::make_unique<Movement>();