#pragma once
#include "../../globals/globals.h"

struct TimeStamp_t {


    int m_iTick{ };
    float m_flFrac{ };

    TimeStamp_t(int tick, const float frac) : m_iTick(tick), m_flFrac(frac) {
        if (m_flFrac < 1.f) {
            if (m_flFrac <= 0.f)
                m_flFrac = 0.f;
        }
        else {
            m_iTick++;
            m_flFrac = 0.f;
        }
    }

    TimeStamp_t(float time) {
        auto temp = std::fmodf(time, 0.015625f);
        m_flFrac = temp * 64;
        m_iTick = TIME_TO_TICKS(time - temp);
        Normalize();
    }

    void Normalize() {
        if (m_flFrac < 1.f) {
            if (m_flFrac <= 0.f)
                m_flFrac = 0.f;
        }
        else {
            m_iTick++;
            m_flFrac = 0.f;
        }
    }

    TimeStamp_t operator-(const TimeStamp_t& other) const {
        const auto frac_temp = m_flFrac - other.m_flFrac;
        auto frac = frac_temp;
        if (frac_temp < 0.f) {
            frac += 1.f;
        }
        auto tick = m_iTick - other.m_iTick - 1;
        if (frac_temp >= 0.f)
            tick = m_iTick - other.m_iTick;
        return { tick, frac };
    }

    TimeStamp_t operator+(const TimeStamp_t& other) const {
        const auto frac_temp = m_flFrac + other.m_flFrac;
        auto frac = frac_temp;
        if (frac_temp > 1.f) {
            frac -= 1.f;
        }
        auto tick = m_iTick + other.m_iTick + 1;
        if (frac_temp <= 1.f)
            tick = m_iTick + other.m_iTick;
        return { tick, frac };
    }

    bool operator<(const TimeStamp_t& other) const {
        if (m_iTick > other.m_iTick)
            return false;
        if (m_iTick >= other.m_iTick)
            return other.m_flFrac > m_flFrac;
        return false;
    }

    float ToTime() const {
        return static_cast<float>(m_iTick) * 0.015625f + m_flFrac * 0.015625;
    }
};

class CGlobalVarsBase {
public:
	float m_real_time; //0x0000
	int32_t m_frame_count; //0x0004
	float m_absolute_frame_time; //0x0008 //
	float m_absolute_frame_start_time_std_dev; //0x000C //
	int32_t m_max_clients; //0x0010
	int16_t m_tickbase; //0x0014
	char pad_0016[22]; //0x0016
	float m_frame_time; //0x002C //
	float m_current_time; //0x0030//
	float m_player_time; //0x0034 //
	char pad_0038[16]; //0x0038
	int32_t m_tick_count; //0x0048 //
	float m_tick_fraction; //0x004C
	char pad_0050[296]; //0x0050
	char* m_map_name; //0x0178
	char* m_map_name_short; //0x0180
};