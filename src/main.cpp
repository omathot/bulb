#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

import app;

/*
	SDL_APP_CONTINUE -> keep running
	SDL_APP_SUCCESS -> terminate successfully
	SDL_APP_FAILUE -> terminate with err
*/

SDL_AppResult SDL_AppInit(void **appstate, int /* argc */, char** /* argv */) {
	// as far as i know this is the right way to do this for SDL, make_unqiue is pointless it requires release() instantly
	App* app = new App(); // NOLINT

	SDL_Init(SDL_INIT_VIDEO);
	app->set_window(SDL_CreateWindow("Test", 800, 600, SDL_WINDOW_RESIZABLE));
	*appstate = app;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
	auto* app = static_cast<App*>(appstate);
	switch (event->type) {
		case SDL_EVENT_KEY_DOWN:
			switch (event->key.key) {
				case SDLK_ESCAPE:
					app->should_exit();
					break;
				default:
					SDL_Log("unhandled key input");
			}
		case SDL_EVENT_KEY_UP:
			SDL_Log("key up: %d", event->key.key);
	}
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
	auto* _ = static_cast<App*>(appstate);
	return SDL_APP_CONTINUE;
}

// NOLINT
void SDL_AppQuit(void *appstate, SDL_AppResult /* result */) {
	auto* app = static_cast<App*>(appstate);
	delete(app); // NOLINT
}
