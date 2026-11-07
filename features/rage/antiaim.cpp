#include "antiaim.h"

void AntiAim::SetYaw(CUserCmd* user_cmd)
{
	if (!Config::anti_aim.overrideYaw)
		return;

	float yaw = user_cmd->csgoUserCmd.mutable_base()->viewangles().y();

	yaw += Config::anti_aim.yawAmount;

	m_should_invert ^= 1;

	yaw += Config::anti_aim.jitterAmount * (m_should_invert ? 0.5f : -0.5f);

	user_cmd->csgoUserCmd.mutable_base()->mutable_viewangles()->set_y(yaw);
}

void AntiAim::SetPitch(CUserCmd* user_cmd)
{
	if (!Config::anti_aim.overridePitch)
		return;

	float pitch = Config::anti_aim.pitchAmount;

	user_cmd->csgoUserCmd.mutable_base()->mutable_viewangles()->set_x(pitch);
}

void AntiAim::OnCreateMove(CUserCmd* user_cmd) {

	if (!Config::anti_aim.enabled)
		return;

	if (!I::EngineClient->in_game())
		return;

	if (!g_ctx->local_pawn ||
		g_ctx->local_pawn->m_iHealth() < 1 ||
		g_ctx->local_pawn->m_MoveType() == MOVETYPE_WALK ||
		g_ctx->local_pawn->m_MoveType() == MOVETYPE_LADDER ||
		user_cmd->nButtons.nValue & IN_ATTACK ||
		user_cmd->nButtons.nValue & IN_USE)
		return;

	SetPitch(user_cmd);
	SetYaw(user_cmd);
}