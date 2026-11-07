#include "movement.h"
#include "../../../../external/protobufs/include/google/protobuf/port.h"
#include "../../utils/math/math.h"
#include "../prediction/prediction.h"

void Movement::Bhop(CUserCmd* user_cmd)
{
    if (!user_cmd) return;

	if (!Config::movement.bhop)
		return;

    C_CSPlayerPawn* pLocalPawn = g_ctx->local_pawn;
    if (!pLocalPawn || pLocalPawn->m_iHealth() <= 0)
        return;

	// Post-prediction flags are the right source wherever the prediction pass
	// runs, but this build never calls C_Prediction::Start/End, so the field
	// stays zero: FL_ONGROUND would never be set and jump would be stripped
	// forever. Fall back to the pawn's own flags, which is exactly what
	// C_Prediction::End copies into that field anyway.
	auto pred = g_prediction->GetPredData();

	uint32_t flags = static_cast<uint32_t>(pred->m_nPostPredictionFlags != 0
		? pred->m_nPostPredictionFlags
		: static_cast<std::int64_t>(pLocalPawn->m_fFlags()));

	if (g_ctx->local_pawn->m_MoveType() == MOVETYPE_LADDER || g_ctx->local_pawn->m_MoveType() == MOVETYPE_NOCLIP)
		return;

	if (!(user_cmd->nButtons.nValue & IN_JUMP || user_cmd->nButtons.nValueScroll & IN_JUMP))
		return;


	user_cmd->nButtons.nValue &= ~IN_JUMP;
	user_cmd->nButtons.nValueChanged &= ~IN_JUMP;
	user_cmd->nButtons.nValueScroll &= ~IN_JUMP;

	if (!(flags & FL_ONGROUND))
		return;

	user_cmd->nButtons.nValue |= IN_JUMP;
	user_cmd->nButtons.nValueScroll |= IN_JUMP;
}

void Movement::AutoStrafe(CUserCmd* user_cmd, Vector_t view_angle)
{
    if (!user_cmd) return;

    C_CSPlayerPawn* pLocalPawn = g_ctx->local_pawn;
    if (!pLocalPawn) return;

    if (pLocalPawn->m_fFlags() & FL_ONGROUND)
        return;

    if (pLocalPawn->m_MoveType() == MOVETYPE_LADDER ||
        pLocalPawn->m_MoveType() == MOVETYPE_NOCLIP)
        return;

    auto base = user_cmd->csgoUserCmd.mutable_base();
    if (!base) return;

    Vector_t velocity = pLocalPawn->m_vecAbsVelocity();
    float speed = velocity.Length2D();

    if (speed < 5.0f)
    {
        base->set_forwardmove(0.3f);
        base->set_leftmove(0.0f);
        return;
    }

    float moveAngleDeg = rad2deg(atan2(velocity.y, velocity.x));
    float delta = std::remainder(view_angle.y - moveAngleDeg, 360.0f);

    float speed_rotation = 30.0f;
    if (speed > 0.0f) {
        speed_rotation = rad2deg(std::asinf(30.0f / speed)) * 0.5f;
        speed_rotation = min(speed_rotation, 45.0f);
        speed_rotation = max(speed_rotation, 5.0f);
    }

    float strafe = 0.0f;

    if (std::abs(delta) > speed_rotation * 0.5f)
    {
        strafe = (delta > 0.0f) ? 1.0f : -1.0f;
    }
    else
    {
        static bool flip = false;
        flip = !flip;
        strafe = flip ? 1.0f : -1.0f;
    }

    base->set_leftmove(strafe);
    base->set_forwardmove(0.0f);
}

void Movement::MovementFix(CUserCmd* user_cmd, Vector_t viewAngle)
{
	Vector_t wish_angle{ user_cmd->csgoUserCmd.mutable_base()->viewangles().x(), user_cmd->csgoUserCmd.mutable_base()->viewangles().y(), user_cmd->csgoUserCmd.mutable_base()->viewangles().z() };
	int sign = wish_angle.x > 89.f ? -1.f : 1.f;
	wish_angle.Clamp();

	Vector_t forward, right, up, old_forward, old_right, old_up;
	Vector_t view_angles = viewAngle;

	c_math->angle_vectors(wish_angle, forward, right, up);

	forward.z = right.z = up.x = up.y = 0.f;

	forward.NormalizeInPlace();
	right.NormalizeInPlace();
	up.NormalizeInPlace();

	c_math->angle_vectors(view_angles, old_forward, old_right, old_up);

	old_forward.z = old_right.z = old_up.x = old_up.y = 0.f;

	old_forward.NormalizeInPlace();
	old_right.NormalizeInPlace();
	old_up.NormalizeInPlace();

	forward *= user_cmd->csgoUserCmd.mutable_base()->forwardmove();
	right *= user_cmd->csgoUserCmd.mutable_base()->leftmove();
	up *= user_cmd->csgoUserCmd.mutable_base()->upmove();

	float fixed_forward_move = old_forward.Dot(right) + old_forward.Dot(forward) + old_forward.DotProduct(up, true);

	float fixed_side_move = old_right.Dot(right) + old_right.Dot(forward) + old_right.DotProduct(up, true);

	float fixed_up_move = old_up.DotProduct(right, true) + old_up.DotProduct(forward, true) + old_up.Dot(up);

	user_cmd->csgoUserCmd.mutable_base()->set_forwardmove(fixed_forward_move);
	user_cmd->csgoUserCmd.mutable_base()->set_leftmove(fixed_side_move);
	user_cmd->csgoUserCmd.mutable_base()->set_upmove(fixed_up_move);

	fixed_forward_move = sign * (old_forward.Dot(right) + old_forward.Dot(forward));
	fixed_side_move = old_right.Dot(right) + old_right.Dot(forward);

	user_cmd->csgoUserCmd.mutable_base()->set_forwardmove(std::clamp(fixed_forward_move, -1.f, 1.f));
	user_cmd->csgoUserCmd.mutable_base()->set_leftmove(std::clamp(fixed_side_move, -1.f, 1.f));
}

void Movement::OnCreateMove(CUserCmd* user_cmd, Vector_t viewAngle)
{
	if (!g_ctx->local_pawn || !user_cmd || !I::EngineClient->in_game() || !I::EngineClient->connected())
		return;

	Bhop(user_cmd);
}
