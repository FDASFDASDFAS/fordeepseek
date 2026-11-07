#pragma once
#include "../C_BaseEntity/C_BaseEntity.h"

// Field names verified against a real working reference implementation's planted-bomb overlay, not
// guessed: m_flC4Blow/m_bHasExploded/m_bBombDefused/m_nBombSite/m_bBeingDefused/m_flTimerLength.
class C_PlantedC4 : public C_BaseEntity
{
public:
	schema(float, m_flC4Blow, "C_PlantedC4->m_flC4Blow");
	schema(bool, m_bHasExploded, "C_PlantedC4->m_bHasExploded");
	schema(bool, m_bBombDefused, "C_PlantedC4->m_bBombDefused");
	schema(int, m_nBombSite, "C_PlantedC4->m_nBombSite");
	schema(bool, m_bBeingDefused, "C_PlantedC4->m_bBeingDefused");
	schema(float, m_flTimerLength, "C_PlantedC4->m_flTimerLength");
};
