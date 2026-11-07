#pragma once

#include "../../globals/globals.h"

class C_Prediction {
public:
    void Start(CUserCmd* pCmd);
    void End(CUserCmd* pCmd);

    struct c_local_data {
        uint32_t m_nTickBase;
        float m_flFrameTime;
        float m_flFrameTime2;
        float m_flIntervalPerSubTick;
        float m_flCurrentTime;
        float m_flCurrentTime2;
        int32_t m_nTickCount;

        float m_nPlayerTickFraction;

        bool m_bInPrediction;
        bool m_bFirstPrediction;
        bool m_bHasBeenPredicted;
        bool m_bShouldPredict;

        std::int64_t m_nPrePredictionFlags;
        std::int64_t m_nPostPredictionFlags;

        float m_nPrePredictionLanding;
        float m_nPostPredictionLanding;

        float m_nSpread;
        float m_nInaccuracy;

        void* m_pMovementServicesBackup;
        Vector_t m_vPreAbsOrigin;
        Vector_t m_vAbsVelocityBackup;
        Vector_t m_vVelocityBackup;
        Vector_t m_vEyePos;

    } m_pred_data{};

    bool m_initialized = false;

    c_local_data* GetPredData() {
        return &m_pred_data;
    }

    float* GetLandindFrac() {
        return &m_pred_data.m_nPostPredictionLanding;
    }
private:
    int m_nLastSequenceProcessed = 0;
};

extern std::unique_ptr<C_Prediction> g_prediction;