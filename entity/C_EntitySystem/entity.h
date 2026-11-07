#pragma once

#include "../../../templeware/globals/globals.h"
#include "../../../templeware/entities/entity_state.h"
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

#define ENTITY_ALL 1
#define ENTITY_ENEMIES_ONLY 2
#define ENTITY_TEAMMATES_ONLY 3

using player_controller_pair = std::pair<CCSPlayerController*, C_CSPlayerPawn*>;

class C_EntitySystem {
    std::unordered_map<uint64_t, std::vector<CEntityInstance*>> entities;
    std::unordered_map<CEntityInstance*, tw::entities::EntityKey> entityKeys;
    tw::entities::EntityRegistry identityRegistry{};
    mutable std::mutex entitiesMutex;
public:
    void initialize();

    void level_init();
    void level_shutdown();
    void add_entity(CEntityInstance*, int);
    void remove_entity(CEntityInstance*, int);
    [[nodiscard]] std::optional<tw::entities::EntityKey> key_for(CEntityInstance*) const;
    [[nodiscard]] std::uint64_t session_epoch() const noexcept;

    std::vector<CEntityInstance*> get(const char*);
    std::vector<C_CSPlayerPawn*> get_players(int);
    std::vector<player_controller_pair> get_players_with_controller(int);
    // One class-name check per live class, not per entity: every tracked class is grouped in its own
    // bucket already, so a bucket's first entity's class name speaks for the whole bucket.
    std::vector<CEntityInstance*> get_matching(const std::function<bool(const char*)>& predicate);
};

inline const auto g_entitySystem = std::make_unique<C_EntitySystem>();
