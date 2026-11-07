#pragma once
#include <cstdint>
#include "../../utils/memory/vfunc/vfunc.h"

class CPrediction
{
public:
    char pad_0000[48];              // 0x0000
    int32_t m_call_reason;          // 0x0030
    bool in_prediction;             // 0x0034
    char pad_0035[3];               // 0x0035
    void* m_current_cmd;    // 0x0038 
    bool engine_pause;              // 0x0040
    char pad_0041[3];               // 0x0041
    int32_t m_snapshot_tick;        // 0x0044
    int32_t m_last_ack_tick;        // 0x0048
    char pad_004C[4];               // 0x004C
    int32_t m_slot_count;           // 0x0050
    char pad_0054[4];               // 0x0054
    void* m_slots;                  // 0x0058
    char pad_0060[40];              // 0x0060
    uint8_t m_frame_snapshot[96];   // 0x0088
    bool in_current_tick;           // 0x00E8
    char pad_00E9[1];               // 0x00E9
    bool supress_prediction;        // 0x00EA
    char pad_00EB[5];               // 0x00EB
    bool first_prediction;          // 0x00F0
    char pad_00F1[3];               // 0x00F1
    bool b_buttons_changed;         // 0x00F4
    char pad_00F5[3];               // 0x00F5
    int32_t m_desired_tick;         // 0x00F8
    float m_desired_when;           // 0x00FC
    char pad_0100[12];              // 0x0100
    bool b_cmd_changed;             // 0x010C
    char pad_010D[11];              // 0x010D
    int32_t m_transmit_count;       // 0x0118
    char pad_011C[20];              // 0x011C
    int32_t m_error_count;          // 0x0130
    char pad_0134[84];              // 0x0134
    bool needs_reinit;              // 0x0188

    void update(int pred_reason)
    {
        M::vfunc<void, 11>(this, pred_reason);
    }
};