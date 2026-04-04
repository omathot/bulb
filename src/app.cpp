module;
#include <SDL3/SDL.h>

module app;
import std;

App::App() {
	if (enableDebug)
		SDL_Log("Starting app in Debug mode");
	else
		SDL_Log("Starting app in release mode");

	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Log("Video driver: %s", SDL_GetCurrentVideoDriver());

	auto* window = SDL_CreateWindow("Test", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
	if (!window) {
		SDL_Log("Failed to create window: %s", SDL_GetError());
	}
	set_window(window);

	auto* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, enableDebug, "vulkan");
	if (!device) {
		SDL_Log("Failed to create GPU Device: %s", SDL_GetError());
	}
	set_device(device); // prefer vulkan

	auto* renderer = SDL_CreateRenderer(get_window(), "vulkan,opengl");
	if (!renderer) {
		SDL_Log("Failed to create renderer: %s", SDL_GetError());
	}
	set_renderer(renderer);
	SDL_Log("graphics backend: %s", SDL_GetRendererName(_renderer));

	SDL_ClaimWindowForGPUDevice(_device, _window);
	setup_gpu_resources();
}

void App::setup_gpu_resources() {
	SDL_GPUBufferCreateInfo bufferInfo {
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = static_cast<Uint32>(vertices.size() * sizeof(Vertex)),
		.props = 0,
	};
	auto* vertexBuff = SDL_CreateGPUBuffer(_device, &bufferInfo);
	SDL_GPUTransferBufferCreateInfo transferInfo {
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = static_cast<Uint32>(vertices.size() * sizeof(Vertex)),
		.props = 0,
	};
	auto* transferBuff = SDL_CreateGPUTransferBuffer(_device, &transferInfo);
	Vertex* data = static_cast<Vertex*>(SDL_MapGPUTransferBuffer(_device, transferBuff, false));
	if (!data) {
		SDL_Log("Failed to map TransferBuffer: %s", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(_device, transferBuff);
		return;
	}
	SDL_memcpy(data, vertices.data(), (vertices.size() * sizeof(Vertex)));
	SDL_UnmapGPUTransferBuffer(_device, transferBuff);
	// TODO: use cmd buff to upload from transferBuff to a GPU VertexBuff
	auto* cmdBuff = SDL_AcquireGPUCommandBuffer(_device);
	auto* copyPass = SDL_BeginGPUCopyPass(cmdBuff);
	SDL_GPUTransferBufferLocation src {
		.transfer_buffer = transferBuff,
		.offset = 0,
	};
	SDL_GPUBufferRegion dst {
		.buffer = vertexBuff,
		.offset = 0,
		.size = static_cast<Uint32>(vertices.size() * sizeof(Vertex)),
	};
	SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(cmdBuff);


	SDL_ReleaseGPUTransferBuffer(_device, transferBuff);
	_vertexBuff = vertexBuff;
}

SDL_Window* App::get_window() const {
	return _window;
}

void App::set_window(SDL_Window* window) {
	_window = window;
}

SDL_Renderer* App::get_renderer() const {
	return _renderer;
}

void App::set_renderer(SDL_Renderer* renderer) {
	_renderer = renderer;
}

SDL_GPUDevice* App::get_device() const {
	return _device;
}

void App::set_device(SDL_GPUDevice* device) {
	_device = device;
}

void App::terminate() {
	_should_exit = true;
}

bool App::should_exit() const {
	return _should_exit;
}

void App::cleanup() {
	SDL_DestroyRenderer(_renderer);
	SDL_ReleaseGPUBuffer(_device, _vertexBuff);
	SDL_DestroyGPUDevice(_device);
	SDL_DestroyWindow(_window);
}

