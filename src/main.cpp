#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

import app;

/*
	SDL_APP_CONTINUE -> keep running
	SDL_APP_SUCCESS -> terminate successfully
	SDL_APP_FAILUE -> terminate with err
*/

SDL_AppResult SDL_AppInit(void **appstate, int /* argc */, char** /* argv */) {
	App* app = new App(); // NOLINT
	*appstate = app;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
	auto* app = static_cast<App*>(appstate);
	switch (event->type) {
		case SDL_EVENT_QUIT:
			app->terminate();
			break;
		case SDL_EVENT_KEY_DOWN:
			switch (event->key.key) {
				case SDLK_ESCAPE:
					app->terminate();
					break;
				default:
					SDL_Log("unhandled key input");
					break;
			}
			break;
		case SDL_EVENT_KEY_UP:
			SDL_Log("key up: %d", event->key.key);
			break;
		default:
			SDL_Log("Unhandled event: %d", event->type);
			break;
	}
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
	auto* app = static_cast<App*>(appstate);
	if (app->should_exit())
		return SDL_APP_SUCCESS;

	SDL_SetRenderDrawColor(app->get_renderer(), 0, 0, 0, 255);
	SDL_RenderClear(app->get_renderer());
	SDL_RenderPresent(app->get_renderer());

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
	if (result == SDL_APP_FAILURE)
		SDL_Log("app returned in failure");
	else if (result == SDL_APP_SUCCESS)
		SDL_Log("app returned in success");

	auto* app = static_cast<App*>(appstate);
	app->cleanup();
	delete(app); // NOLINT
}
