#pragma once

namespace tw::events
{
    // Registers our listener with CGameEventManager (client.dll).
    // Requires interfaces to be initialised first. Returns false if the
    // manager could not be resolved.
    bool init();

    // MUST run before the module is freed. The game stores a raw pointer to
    // our listener object; if we unload without removing it, the next event
    // dispatch calls through a vtable in freed memory.
    void shutdown();
}
