#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

import app;
/*
	SDL_APP_CONTINUE -> keep running
	SDL_APP_SUCCESS -> terminate successfully
	SDL_APP_FAILUE -> terminate with err
*/

SDL_AppResult SDL_AppInit(void **appstate, int argc, char** argv) {
	App* app = new App(); // NOLINT
	if (argc > 0) {
		app->arguments(argc, argv);
	}
	app->init();
	*appstate = app;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
	auto* app = static_cast<App*>(appstate);
	SDL_AppResult res = app->handle_event(event);
	return res;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
	auto* app = static_cast<App*>(appstate);
	SDL_AppResult res = app->iterate();
	return res;
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
