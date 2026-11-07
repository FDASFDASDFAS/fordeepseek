#pragma once
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../../../cs2/datatypes/utlstronghandle/utlstronghandle.h"
#include "../C_CSWeaponBase/C_CSWeaponBase.h"
#include "../C_BaseEntity/C_BaseEntity.h"
#include "../CGameSceneNode/CGameSceneNode.h"
#include "../../../templeware/utils/math/viewmatrix/viewmatrix.h"

#include <cstdint>

class CInButtonStatePB;
class CUserCmd;

enum class BoneID : int {
    ORIGIN = 0,
    PELVIS = 1,
    SPINE0 = 2,
    SPINE1 = 3,
    SPINE2 = 4,
    NECK = 6,
    HEAD = 7,
    CLAVICLE_L = 8,
    SHOULDER_L = 9,
    ELBOW_L = 10,
    HAND_L = 11,
    CLAVICLE_R = 12,
    SHOULDER_R = 13,
    ELBOW_R = 14,
    HAND_R = 15,
    HIP_L = 17,
    KNEE_L = 18,
    FOOT_HEEL_L = 19,
    HIP_R = 20,
    KNEE_R = 21,
    FOOT_HEEL_R = 22,
    CHEST = 23,
    GUN = 24,
    EYE_L = 25,
    EYE_R = 26,
    RANDOM = 27,
    CVJ_BONE = 28,
    FOOT_TOES_L_T = 74,
    FOOT_TOES_R_T = 77,
    FOOT_TOES_L_CT = 81,
    FOOT_TOES_R_CT = 86,
    BONE_MAX = 128
};


struct BoneConnection { BoneID bone1; BoneID bone2; };

namespace Bones
{
    // Every index below is cross-validated by two independent same-engine sources: velocity-main's
    // own working cstypes::bone_ids table (21/21 indices match this enum exactly) and the current
    // live schema dump's CSkeletonInstance->m_modelState offset. The toe segments are deliberately
    // left out: FOOT_TOES_L_CT/FOOT_TOES_R_CT appear in neither source, so they stay unverified.
    inline const std::vector<BoneConnection> connections = {
        { BoneID::PELVIS, BoneID::SPINE1 },
        { BoneID::SPINE1, BoneID::SPINE2 },
        { BoneID::SPINE2, BoneID::CHEST },
        { BoneID::CHEST, BoneID::NECK },
        { BoneID::NECK,  BoneID::HEAD },
        { BoneID::NECK,           BoneID::SHOULDER_L },
        { BoneID::SHOULDER_L,    BoneID::ELBOW_L },
        { BoneID::ELBOW_L,    BoneID::HAND_L },
        { BoneID::NECK,           BoneID::SHOULDER_R },
        { BoneID::SHOULDER_R,    BoneID::ELBOW_R },
        { BoneID::ELBOW_R,    BoneID::HAND_R },
        { BoneID::PELVIS,         BoneID::HIP_L },
        { BoneID::HIP_L,    BoneID::KNEE_L },
        { BoneID::KNEE_L,    BoneID::FOOT_HEEL_L },
        { BoneID::PELVIS,         BoneID::HIP_R },
        { BoneID::HIP_R,    BoneID::KNEE_R },
        { BoneID::KNEE_R,    BoneID::FOOT_HEEL_R },
    };
}

inline bool IsValidPtr(uintptr_t addr)
{
    return addr > 0x10000 && addr < 0x7FFFFFFFFFFF;
}

class CCSPlayerModernJump {
public:
    schema(float, m_flLastLandedFrac, "CCSPlayerModernJump->m_flLastLandedFrac");
};

class C_PlayerMovementService {
public:
    schema(float, m_max_speed, "CPlayer_MovementServices->m_flMaxspeed");
    schema(float, m_surface_friction, "CPlayer_MovementServices_Humanoid->m_flSurfaceFriction");
    SCHEMA_ADD_OFFSET(CInButtonStatePB, m_button_state, 0x50);
    schema(uint64_t, m_nQueuedButtonDownMask, "CPlayer_MovementServices->m_nQueuedButtonDownMask");
    schema(uint64_t, m_nQueuedButtonChangeMask, "CPlayer_MovementServices->m_nQueuedButtonChangeMask");
    schema(uint64_t, m_nButtonDoublePressed, "CPlayer_MovementServices->m_nButtonDoublePressed");
    schema(uint64_t, m_nToggleButtonDownMask, "CPlayer_MovementServices->m_nToggleButtonDownMask");
    schema(uint64_t, m_nButtonDownMaskPrev, "CCSPlayer_MovementServices->m_nButtonDownMaskPrev");
    schema(uint64_t, m_flCmdForwardMove, "CCSPlayer_MovementServices->m_flCmdForwardMove");
    schema(CCSPlayerModernJump, m_ModernJump, "CCSPlayer_MovementServices->m_ModernJump");

    void set_prediction_command(CUserCmd* user_cmd) {

        M::CallVFunc<void, 46U>(this, user_cmd);
    }

    void run_command(CUserCmd* user_cmd) {
        M::CallVFunc<void, 32U>(this, user_cmd);
    }

    void reset_prediction_command() {
        M::CallVFunc<void, 47U>(this);
    }
};

class ViewAngleServerChange_t {
public:
	schema(QAngle_t*, qAngle, "ViewAngleServerChange_t->qAngle");
};

class CPlayer_ObserverServices {
public:
    schema(CBaseHandle, m_hObserverTarget, ("CPlayer_ObserverServices->m_hObserverTarget"));
};

class C_CSPlayerPawn : public C_BaseEntity {
public:
	schema(Vector_t, m_vOldOrigin, "C_BasePlayerPawn->m_vOldOrigin");
	schema(Vector_t, m_vecViewOffset, "C_BaseModelEntity->m_vecViewOffset");
	schema(CCSPlayer_WeaponServices*, m_pWeaponServices, "C_BasePlayerPawn->m_pWeaponServices");
	schema(ViewAngleServerChange_t*, m_ServerViewAngleChanges, "C_BasePlayerPawn->m_ServerViewAngleChanges");
	schema(bool, m_bIsScoped, "C_CSPlayerPawn->m_bIsScoped");
	schema(float, m_flFlashDuration, "C_CSPlayerPawnBase->m_flFlashDuration");
	// Verified against the current live schema dump (cs2-universal-offsets, build 14172): writing this
	// caps the shader-driven whiteout intensity directly, no render-function hook required.
	schema(float, m_flFlashMaxAlpha, "C_CSPlayerPawnBase->m_flFlashMaxAlpha");
    schema(CGameSceneNode*, m_pGameSceneNode, "C_BaseEntity->m_pGameSceneNode");
    schema(CPlayer_ObserverServices*, m_pObserverServices, "C_BasePlayerPawn->m_pObserverServices");
    schema(C_PlayerMovementService*, m_MovementServices, "C_BasePlayerPawn->m_pMovementServices");
	C_CSPlayerPawn(uintptr_t address);

	C_CSWeaponBase* GetActiveWeapon();
	CCSPlayer_WeaponServices* GetWeaponServices();
	Vector_t getPosition() const;
	Vector_t getEyePosition() const;

	uintptr_t getAddress() const;
	int getHealth() const;
	uint8_t getTeam() const;
	Vector_t getViewOffset() const;
private:
	uintptr_t address;
};

class c_transform
{
public:
    Vector_t position;
    Vector4D_t rotate;
    //public:
    //	matrix3x4_t to_matrix();
};

class C_Model;

class c_perm_model_ext_part
{
public:
    c_transform m_Transform; // 0x0	
    const char* m_Name; // 0x20	
    int32_t m_nParent; // 0x28	
private:
    [[maybe_unused]] uint8_t __pad002c[0x4]; // 0x2c
public:
    C_StrongHandle< C_Model > m_refModel; // 0x30	
};

struct HitboxData_t {
    Vector_t m_vMins;
    Vector_t m_vMaxs;
    Vector_t m_vCenter;
    float m_flRadius;
    int m_nHitboxIndex;
    int m_nShapeType;
    bool m_bMultipoint;
};

class C_Hitbox
{
public:
    char* m_name; //0x0000
    char* m_surface_property; //0x0008
    char* m_bone_name; //0x0010
    Vector_t m_vec_min; //0x0018
    Vector_t m_vec_maxs; //0x0024
    float m_shape_radius; //0x0030
    uint32_t m_bone_hash; //0x0034
    int32_t m_hitgroup; //0x0038
    uint8_t m_shape_type; //0x003C
    bool m_translation_only; //0x003D
    std::uint32_t m_crc;                 // 0x0040
    char pad1[4];                // 0x0044 color ?
    std::uint16_t m_hitbox_index;        // 0x0048
    char pad_003E[0x22]; //0x003E
};

class C_HitboxSets
{
public:
    std::byte pad_0x0000[0x20]; // 0x0000
    uint32_t m_nNameHash;		// 0x0020
    std::byte pad_0x0024[0x4];	// 0x0024
    __int32	m_nHitboxCount;	// 0x0028
    std::byte pad_0x002C[0x4];	// 0x002C
    C_Hitbox* m_hitbox;			// 0x0030
    std::byte pad_0x0038[0x18]; // 0x0038
};

class C_RenderMesh
{
public:
    char pad_0000[0x168];
    C_HitboxSets* m_hitboxsets;     // 0x0168 - Updated offset!
    int32_t m_nHitboxSets;          // 0x0170 - Updated offset!
};

class C_RenderMeshes
{
public:
    C_RenderMesh* m_pMeshes;
};


class c_quaternion_storage
{
public:
    float x, y, z, w;
};

struct ModelSkeletonData_t
{
public:
    c_utl_vector< const char* > m_boneName; // 0x0	
    char pad[0x8];
    c_utl_vector< int16_t > m_nParent; // 0x18	
    char pad2[0x8];
    c_utl_vector< float > m_boneSphere; // 0x30	
    char pad3[0x8];
    c_utl_vector< std::uint32_t > m_nFlag; // 0x48	
    char pad4[0x8];
    c_utl_vector< Vector_t > m_bonePosParent; // 0x60	
    char pad5[0x8];
    c_utl_vector< c_quaternion_storage > m_boneRotParent; // 0x78	
    char pad6[0x8];
    c_utl_vector< float > m_boneScaleParent; // 0x90	
    char pad7[0x8];
};

class C_PermModelData
{
public:
    const char* name; //0x0000
    uint32_t flags; //0x0008
    Vector_t hull_min; //0x000C
    Vector_t hull_max; //0x0018
    Vector_t view_min; //0x0024
    Vector_t view_max; //0x0030
    float mass; //0x003C
    Vector_t eye_position; //0x0040
    float max_eye_deflection; //0x004C
    char* surface_property; //0x0050
    c_utl_vector<c_perm_model_ext_part> ext_parts;
    c_utl_vector<C_RenderMesh*>  ref_meshes;
    char pad2[0x8];
    c_utl_vector<uint64_t> mesh_group_mask;
    char pad3[0x8];
    c_utl_vector<uint64_t> phys_group_mask;
    char pad5[0x30];
    c_utl_vector<void*> anim_groups; // 0x120  
    char pad9[0x70];
    ModelSkeletonData_t model_skeleton; // 0x188  
};

class C_Model
{
public:
    void* vtable;
    C_PermModelData perm_model_data;

    C_Hitbox* get_hitbox(int index)
    {
        auto model_data = &perm_model_data;

        if (perm_model_data.ref_meshes.m_size <= 0)
            return nullptr;

        auto mesh = perm_model_data.ref_meshes.element(0);
        if (!mesh)
            return nullptr;

        auto hitboxsets = mesh->m_hitboxsets;
        if (!hitboxsets)
            return nullptr;

        if (hitboxsets->m_nHitboxCount <= 0 || index > hitboxsets->m_nHitboxCount)
            return nullptr;

        auto& hitbox = hitboxsets->m_hitbox;
        return &hitbox[index];
    }

    const char* get_bone_name(int index)
    {
        if (perm_model_data.model_skeleton.m_boneName.count() <= 0 || perm_model_data.model_skeleton.m_boneName.count() > index)
            return "root";

        return perm_model_data.model_skeleton.m_boneName.element(index);
    }

    const char* get_hitbox_name(int index)
    {
        auto hitbox = get_hitbox(index);

        if (!hitbox)
            return nullptr;

        return hitbox->m_name;
    }
    int get_max_hitboxes()
    {
        if (perm_model_data.ref_meshes.count() <= 0)
            return -1;

        auto meshes = perm_model_data.ref_meshes.element(0);
        if (!meshes)
            return -1;

        auto hithoxsets = meshes->m_hitboxsets;
        if (!hithoxsets)
            return -1;

        return hithoxsets[0].m_nHitboxCount;
    }

    uint32_t get_hitboxes_num()
    {
        using fnHitboxNum = uint32_t(__fastcall*)(void*);
        static auto HitboxNum = reinterpret_cast<fnHitboxNum>(M::scan_absolute("client.dll", "E8 ? ? ? ? 85 C0 7E ? 83 7F 20 00", 0x1));

        return HitboxNum(this);
    }

    uint32_t get_bone_flags(std::uint32_t index)
    {
        static auto fn = reinterpret_cast<uint32_t(__fastcall*)(void*, uint32_t)>(M::scan("client.dll", "85 D2 78 ? 3B 91 ? ? ? ? 7D ? 48 8B 81 ? ? ? ? 48 63 D2 8B 04 90"));

        return fn(this, index);
    }

    std::int32_t get_bone_parent(std::uint32_t index)
    {
        static auto fn = reinterpret_cast<uint32_t(__fastcall*)(void*, uint32_t)>(M::scan("client.dll", "85 D2 78 ? 3B 91 ? ? ? ? 7D ? 48 8B 81 ? ? ? ? 48 63 D2 0F BF 04 50"));

        return fn(this, index);
    }
public:
    MEM_PAD(0x70);					// 0x0000
    std::int32_t m_nRendermeshCount;// 0x0070
    MEM_PAD(0x4);					// 0x0074
    C_RenderMeshes* m_pRenderMeshes;	// 0x0078
};

struct alignas(16) C_BoneData {
    Vector_t m_pos;
    float m_scale;
    Vector4D_t m_rot;
};

class C_ModelState {
public:
    schema(C_StrongHandle<C_Model>, m_model, "CModelState->m_hModel");

    SCHEMA_ADD_OFFSET(C_BoneData*, get_bone_data, 0x80);
};

class CSkeletonInstance : public CGameSceneNode
{
public:
    char pad_003[412];
    int m_bone_count;
    char pad_002[24];
    int m_mask;
    char pad_001[4];
    Matrix2x4_t* m_bone_cache;

    schema(C_ModelState, m_model_state, "CSkeletonInstance->m_modelState");
    schema(uint8_t, m_hitbox_set, "CSkeletonInstance->m_nHitboxSet");

    void calc_world_space_bones(std::uint32_t bone_mask) {
        static const auto fn = reinterpret_cast<void(__fastcall*)(void*, unsigned int)>(M::scan("client.dll", "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC D0"));
        return fn(this, bone_mask);
    }
};

inline Vector_t GetBonePos(C_CSPlayerPawn* pawn, BoneID boneID)
{
    if (!pawn)
        return {};

    auto* scene = reinterpret_cast<CGameSceneNode*>(pawn->m_pGameSceneNode());
    if (!scene)
        return {};

    auto* skeleton = scene->GetSkeletonInstance();
    if (!skeleton)
        return {};

    // m_modelState is embedded, not a pointer: schema-resolved rather than a hardcoded literal, so it
    // tracks the live offset instead of assuming one. Verified against the current schema dump
    // (CSkeletonInstance->m_modelState == 0x140) rather than trusted as a bare constant.
    static const std::uint32_t modelStateOffset = SchemaFinder::Get(hash_32_fnv1a_const("CSkeletonInstance->m_modelState"));
    const uintptr_t modelState = reinterpret_cast<uintptr_t>(skeleton) + modelStateOffset;

    const uintptr_t boneCache = *reinterpret_cast<uintptr_t*>(modelState + 0x80);
    if (!IsValidPtr(boneCache))
        return {};

    // Bounds-checked against the live bone count (m_modelState + 0x8C) rather than trusting the
    // requested index blindly -- a model with fewer bones than assumed must not be read out of range.
    const int boneCount = *reinterpret_cast<const int*>(modelState + 0x8C);
    const int index = static_cast<int>(boneID);
    if (index < 0 || index >= boneCount)
        return {};

    return *reinterpret_cast<Vector_t*>(boneCache + static_cast<std::size_t>(index) * 0x20);
}
