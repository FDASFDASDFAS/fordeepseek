#pragma once
#include "../../../templeware/utils/schema/schema.h"
#include "../../../templeware/utils/memory/vfunc/vfunc.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../C_EntityInstance/C_EntityInstance.h"

class CSkeletonInstance;

// Generic scene-node origin/rotation for any C_BaseEntity, not only player pawns. Position-only
// consumers (item/projectile/bomb overlays) are not blocked by the still-unresolved semantic bone
// discovery -- only GetSkeletonInstance()'s bone-cache callers are.
class CGameSceneNode
{
public:
    schema(CEntityInstance*, GetOwner, "CGameSceneNode->m_pOwner");

    schema(Vector_t, GetAbsOrigin, "CGameSceneNode->m_vecAbsOrigin");
    schema(Vector_t, GetVecOrigin, "CGameSceneNode->m_vecOrigin");
    schema(Vector_t, GetRenderOrigin, "CGameSceneNode->m_vRenderOrigin");
    schema(QAngle_t, GetAngleRotation, "CGameSceneNode->m_angRotation");
    schema(QAngle_t, GetAbsAngleRotation, "CGameSceneNode->m_angAbsRotation");
    schema(CGameSceneNode*, GetChild, "CGameSceneNode->m_pChild");
    schema(CGameSceneNode*, GetNextSibling, "CGameSceneNode->m_pNextSibling");
    schema(bool, IsDormant, "CGameSceneNode->m_bDormant");

    CSkeletonInstance* GetSkeletonInstance()
    {
        return M::CallVFunc<CSkeletonInstance*, 13U>(this);
    }
};
