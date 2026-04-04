#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
		SDL_SubmitGPUCommandBuffer(cmdBuff);
		return SDL_APP_CONTINUE;
	}
	SDL_GPUColorTargetInfo colorInfo {
		.texture = swapchain,
		.clear_color = {.r = 0.1f, .g=0.01f, .b=0.1f, .a = 1.0f},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE,
	};
	auto* renderPass = SDL_BeginGPURenderPass(cmdBuff, &colorInfo, 1, nullptr);
	SDL_BindGPUGraphicsPipeline(renderPass, app->get_graphics_pipeline());

	// push constants
	float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
	glm::mat4 proj = glm::perspectiveZO(glm::radians(45.0f), aspect, 0.1f, 100.0f);
	static float angle = 0.0f;
	angle += 0.01f;
	proj[1][1] *= -1;
	UniformBuffer ubo {
		.model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)),
		.view = glm::lookAt(
			glm::vec3(0.0f, 0.0f, 2.0f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
		),
		.proj = proj
	};
	SDL_PushGPUVertexUniformData(cmdBuff, 0, &ubo, sizeof(ubo));

	// bindings
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

	// draw
	SDL_DrawGPUIndexedPrimitives(renderPass, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
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
