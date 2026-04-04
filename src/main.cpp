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
					SDL_Log("key down: %s", SDL_GetKeyName(event->key.key));
					break;
			}
			break;
		case SDL_EVENT_KEY_UP:
			SDL_Log("key up: %s", SDL_GetKeyName(event->key.key));
			break;
		default:
			char desc[256];
			SDL_GetEventDescription(event, static_cast<char*>(desc), sizeof(desc));
			SDL_Log("Unhandled event: %s", static_cast<char*>(desc));
			break;
	}
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
	auto* app = static_cast<App*>(appstate);
	if (app->should_exit())
		return SDL_APP_SUCCESS;

	auto* cmdBuff = SDL_AcquireGPUCommandBuffer(app->get_device());
	SDL_GPUTexture* swapchain = nullptr;
	SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuff, app->get_window(), &swapchain, nullptr, nullptr);
	if (!swapchain) {
		SDL_Log("Failed to acquire GPU Swapchain Texture: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	SDL_GPUColorTargetInfo colorInfo {
		.texture = swapchain,
		.clear_color = {.r = 0.0f, .g=0.0f, .b=0.0f, .a = 1.0f},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE,
	};
	auto* renderPass = SDL_BeginGPURenderPass(cmdBuff, &colorInfo, 1, nullptr);
	// bind pipeline, vertex/index buff, draw
	SDL_GPUBufferBinding indexBinding {
		.buffer = app->get_index_buff(),
		.offset = 0,
	};
	SDL_GPUBufferBinding vertexBinding {
		.buffer = app->get_vertex_buff(),
		.offset = 0,
	};
	SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
	SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
	SDL_EndGPURenderPass(renderPass);
	SDL_SubmitGPUCommandBuffer(cmdBuff);

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
