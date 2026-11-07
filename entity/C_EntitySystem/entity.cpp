#include "entity.h"

#include <algorithm>

void C_EntitySystem::initialize()
{
    std::unordered_map<uint64_t, std::vector<CEntityInstance*>> initializedEntities;
    std::unordered_map<CEntityInstance*, tw::entities::EntityKey> initializedKeys;
    int highest_idx = I::EntitySystem->get_highest_entity_index();

    for (int i = 0; i <= highest_idx; i++) {
        CEntityInstance* entity = I::EntitySystem->get_base_entity(i);

        if (!entity)
            continue;

        const char* class_name = entity->get_entity_class_name();
        if (!class_name)
            continue;
        auto key = identityRegistry.observeAdd(static_cast<std::uint32_t>(i));
        if (!key)
            continue;
        initializedEntities[fnv1a::hash_64(class_name)].emplace_back(entity);
        initializedKeys.emplace(entity, key.value());
    }

    std::scoped_lock lock(entitiesMutex);
    entities = std::move(initializedEntities);
    entityKeys = std::move(initializedKeys);
}

void C_EntitySystem::level_init() {
    level_shutdown();
    initialize();
}

void C_EntitySystem::level_shutdown() {
    identityRegistry.advanceEpoch();
    std::scoped_lock lock(entitiesMutex);
    entities.clear();
    entityKeys.clear();
}

void C_EntitySystem::add_entity(CEntityInstance* entity, int index) {
    if (!entity)
        return;
    const char* className = entity->get_entity_class_name();
    if (!className)
        return;
    const auto entityIndex = static_cast<std::uint32_t>(entity->handle().index());
    auto key = identityRegistry.observeAdd(entityIndex);
    if (!key)
        return;

    std::scoped_lock lock(entitiesMutex);
    entities[fnv1a::hash_64(className)].emplace_back(entity);
    entityKeys[entity] = key.value();
}

void C_EntitySystem::remove_entity(CEntityInstance* entity, int index) {
    if (!entity)
        return;
    const char* class_name = entity->get_entity_class_name();
    if (!class_name)
        return;

    std::optional<tw::entities::EntityKey> removedKey;
    {
        std::scoped_lock lock(entitiesMutex);
        auto key = entityKeys.find(entity);
        if (key != entityKeys.end())
        {
            removedKey = key->second;
            entityKeys.erase(key);
        }
        auto it = entities.find(fnv1a::hash_64(class_name));
        if (it != entities.end())
        {
            std::vector<CEntityInstance*>& classEntities = it->second;
            classEntities.erase(std::remove(classEntities.begin(), classEntities.end(), entity), classEntities.end());
        }
    }

    if (removedKey)
        identityRegistry.observeRemove(*removedKey);
}

std::optional<tw::entities::EntityKey> C_EntitySystem::key_for(CEntityInstance* entity) const {
    std::optional<tw::entities::EntityKey> key;
    {
        std::scoped_lock lock(entitiesMutex);
        const auto found = entityKeys.find(entity);
        if (found != entityKeys.end())
            key = found->second;
    }
    if (!key || !identityRegistry.valid(*key))
        return std::nullopt;
    return key;
}

std::uint64_t C_EntitySystem::session_epoch() const noexcept {
    return identityRegistry.epoch();
}

std::vector<CEntityInstance*> C_EntitySystem::get(const char* name) {
    std::scoped_lock lock(entitiesMutex);
    const auto found = entities.find(fnv1a::hash_64(name));
    return found != entities.end() ? found->second : std::vector<CEntityInstance*>{};
}

std::vector<CEntityInstance*> C_EntitySystem::get_matching(const std::function<bool(const char*)>& predicate) {
    std::vector<CEntityInstance*> result;
    std::scoped_lock lock(entitiesMutex);
    for (const auto& [hash, bucket] : entities) {
        if (bucket.empty())
            continue;
        const char* className = bucket.front()->get_entity_class_name();
        if (className && predicate(className))
            result.insert(result.end(), bucket.begin(), bucket.end());
    }
    return result;
}

std::vector<C_CSPlayerPawn*> C_EntitySystem::get_players(int index) {
    std::vector<C_CSPlayerPawn*> enteties;

    C_CSPlayerPawn* local_player = g_ctx->local_pawn;
    if (!local_player)
        return enteties;

    int local_team_num = local_player->m_iTeamNum();

    std::vector<CEntityInstance*> entities = get("C_CSPlayerPawn");
    for (CEntityInstance* entity : entities) {
        C_CSPlayerPawn* player_pawn = reinterpret_cast<C_CSPlayerPawn*>(entity);

        int team_num = player_pawn->m_iTeamNum();

        if (index == ENTITY_ALL || ((index == ENTITY_ENEMIES_ONLY && local_team_num != team_num) || (index == ENTITY_TEAMMATES_ONLY && local_team_num == team_num && local_player != player_pawn)))
            enteties.emplace_back(player_pawn);
    }

    return enteties;
}

std::vector<player_controller_pair> C_EntitySystem::get_players_with_controller(int index) {
    std::vector<player_controller_pair> return_enteties;

    C_CSPlayerPawn* local_player = g_ctx->local_pawn;
    if (!local_player)
        return return_enteties;

    int local_team_num = local_player->m_iTeamNum();

    std::vector<CEntityInstance*> entities = get("CCSPlayerController");
    for (auto entity : entities)
    {
        CCSPlayerController* controller = reinterpret_cast<CCSPlayerController*>(entity);
        if (!controller)
            continue;

        C_CSPlayerPawn* player_pawn = I::GameEntity->Instance->Get<C_CSPlayerPawn>(controller->m_hPawn());
        if (!player_pawn)
            continue;

        int team_num = player_pawn->m_iTeamNum();

        if (index == ENTITY_ALL || ((index == ENTITY_ENEMIES_ONLY && local_team_num != team_num) || (index == ENTITY_TEAMMATES_ONLY && local_team_num == team_num && local_player != player_pawn)))
            return_enteties.emplace_back(std::make_pair(controller, player_pawn));
    }

    return return_enteties;
}
