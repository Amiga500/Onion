# C source layout

- `app/`: entry point, event dispatch, browser-row/navigation controller and details controller.
- `core/`: platform-independent config, viewport, incremental letter navigation and state codec.
- `catalog/`: ROM/cache discovery, saved lists and transactional saved-list actions.
- `menus/`: menu definitions, configuration and stock setting models.
- `ui/`: theme resources, frame/header/footer, list panels, dialogs, grids and thumbnails.
- `platform/`: input, bounded file I/O, system config and optional SDL_mixer audio.
- `localization/`: on-demand custom language loading and translation lookup.

Include project headers by their domain path, such as `catalog/catalog.h`. Keep dependencies directed from app/UI to models and platform boundaries. The portable core must not depend on SDL.

New screens should get their own controller in `app/` to keep `app/screen_events.c` focused on event dispatch. `app/main.c` coordinates startup, the main loop and teardown for both host and device builds.

`app/session.c` owns source snapshots and stable restoration; `app/search.c` owns bounded contextual results; `app/settings_page.c` owns Display/Wi-Fi/About state. `platform/launch.c` publishes compatible launch files `platform/device_request.c` builds injected Wi-Fi/shutdown requests.

These modules do not execute SD scripts.
