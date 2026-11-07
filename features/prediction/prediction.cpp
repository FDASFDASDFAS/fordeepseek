#include "prediction.h"

std::unique_ptr<C_Prediction> g_prediction = std::make_unique<C_Prediction>();

void C_Prediction::Start(CUserCmd* pCmd)
{
	m_initialized = false;
	if (!pCmd || !g_ctx || !g_ctx->local_pawn || !g_ctx->local_controller || !I::NetworkClient ||
		!I::GlobalVars || !I::EngineClient || !I::Prediction)
		return;

	if (pCmd->m_nCommandNumber == this->m_nLastSequenceProcessed)
		return;

	C_PlayerMovementService* pMovementServices = g_ctx->local_pawn->m_MovementServices();
	if (!pMovementServices)
		return;

	INetworkClient* pNetworkGameClient = I::NetworkClient->get_network_client();
	if (!pNetworkGameClient)
		return;

	C_CSWeaponBase* pWeapon = g_ctx->local_pawn->GetActiveWeapon();
	if (!pWeapon)
		return;

    if (g_ctx->local_pawn->m_iHealth() <= 0)
        return;

    m_pred_data.m_vAbsVelocityBackup = g_ctx->local_pawn->m_vecAbsVelocity();
    m_pred_data.m_vVelocityBackup = g_ctx->local_pawn->m_vecVelocity();
    m_pred_data.m_vPreAbsOrigin = g_ctx->local_pawn->m_vecAbsVelocity();

    m_pred_data.m_flIntervalPerSubTick = I::GlobalVars->m_frame_time;
    m_pred_data.m_flCurrentTime = I::GlobalVars->m_current_time;
    m_pred_data.m_flCurrentTime2 = I::GlobalVars->m_player_time;
    m_pred_data.m_nTickCount = I::GlobalVars->m_tick_count;
    m_pred_data.m_flFrameTime = I::GlobalVars->m_frame_time;
    m_pred_data.m_flFrameTime2 = I::GlobalVars->m_absolute_frame_start_time_std_dev;
    m_pred_data.m_nPrePredictionFlags = g_ctx->local_pawn->m_fFlags();
    m_pred_data.m_nPrePredictionLanding = pMovementServices->m_ModernJump().m_flLastLandedFrac();

    m_pred_data.m_nTickBase = g_ctx->local_controller->m_nTickBase();
    const auto clientInfo = I::EngineClient->get_networked_client_info();
    if (!clientInfo.m_local_data)
        return;
    m_pred_data.m_nPlayerTickFraction = clientInfo.m_player_tick_fraction;

    m_pred_data.m_bInPrediction = I::Prediction->in_prediction;
    m_pred_data.m_bFirstPrediction = I::Prediction->first_prediction;
    m_pred_data.m_bHasBeenPredicted = pCmd->m_bHasBeenPredicted;
    m_pred_data.m_bShouldPredict = pNetworkGameClient->m_bShouldPredict;

    m_pred_data.m_nSpread = pWeapon->get_spread();
    m_pred_data.m_nInaccuracy = pWeapon->get_inaccuracy();

    pCmd->m_bHasBeenPredicted = false;
    pNetworkGameClient->m_bShouldPredict = true;
    I::Prediction->first_prediction = false;
    I::Prediction->in_prediction = true;

    pMovementServices->set_prediction_command(pCmd);

    m_initialized = true;
    pMovementServices->run_command(pCmd);


    Vector_t vNonInterpolatedShootPos = clientInfo.m_local_data->m_eye_pos;

    using fnCalculateShootPosition = void(__fastcall*)(Vector_t*, C_CSPlayerPawn*, TimeStamp_t*, void*, void*);
    static fnCalculateShootPosition CalculateShootPosition = (fnCalculateShootPosition)(M::FindPattern("client", "48 89 5C 24 ? 48 89 6C 24 ? 56 57 41 56 48 83 EC ? 44 8B 92"));
    TimeStamp_t objTimeStamp = { (int)g_ctx->local_controller->m_nTickBase(), 0.f };

    if (CalculateShootPosition)
        CalculateShootPosition(&vNonInterpolatedShootPos, g_ctx->local_pawn, &objTimeStamp, nullptr, nullptr);

    m_pred_data.m_vEyePos = vNonInterpolatedShootPos;

    pMovementServices->reset_prediction_command();

    g_ctx->local_pawn->m_vecAbsVelocity() = m_pred_data.m_vAbsVelocityBackup;
    g_ctx->local_pawn->m_vecVelocity() = m_pred_data.m_vVelocityBackup;
}

void C_Prediction::End(CUserCmd* pCmd) {
    if (!m_initialized || !pCmd || !g_ctx || !g_ctx->local_pawn || !g_ctx->local_controller ||
        !I::NetworkClient || !I::GlobalVars || !I::Prediction)
        return;

    if (pCmd->m_nCommandNumber == this->m_nLastSequenceProcessed)
        return;

    C_PlayerMovementService* pMovementServices = g_ctx->local_pawn->m_MovementServices();
    if (!pMovementServices)
        return;

    INetworkClient* pNetworkGameClient = I::NetworkClient->get_network_client();
    if (!pNetworkGameClient)
        return;

    I::GlobalVars->m_frame_time = m_pred_data.m_flIntervalPerSubTick;
    I::GlobalVars->m_current_time = m_pred_data.m_flCurrentTime;
    I::GlobalVars->m_player_time = m_pred_data.m_flCurrentTime2;
    I::GlobalVars->m_tick_count = m_pred_data.m_nTickCount;
    I::GlobalVars->m_frame_time = m_pred_data.m_flFrameTime;
    I::GlobalVars->m_absolute_frame_start_time_std_dev = m_pred_data.m_flFrameTime2;

    g_ctx->local_controller->m_nTickBase() = m_pred_data.m_nTickBase;

    I::Prediction->first_prediction = m_pred_data.m_bFirstPrediction;
    I::Prediction->in_prediction = m_pred_data.m_bInPrediction;
    pCmd->m_bHasBeenPredicted = m_pred_data.m_bHasBeenPredicted;
    pNetworkGameClient->m_bShouldPredict = m_pred_data.m_bShouldPredict;

    m_pred_data.m_nPostPredictionFlags = g_ctx->local_pawn->m_fFlags();
    m_pred_data.m_nPostPredictionLanding = pMovementServices->m_ModernJump().m_flLastLandedFrac();

    this->m_nLastSequenceProcessed = pCmd->m_nCommandNumber;
    m_initialized = false;
}
