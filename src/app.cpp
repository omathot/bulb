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
	SDL_PropertiesID deviceProps = SDL_GetGPUDeviceProperties(_device);
	const auto* name = SDL_GetStringProperty(deviceProps, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown");
	SDL_Log("Picked device: %s", name);
	SDL_Log("using driver: %s", SDL_GetGPUDeviceDriver(_device));

	SDL_ClaimWindowForGPUDevice(_device, _window);
	setup_gpu_resources();
}

void App::setup_gpu_resources() {
	std::uint32_t vertexSize = static_cast<Uint32>(vertices.size() * sizeof(Vertex));
	std::uint32_t indexSize = static_cast<Uint32>(indices.size() * sizeof(std::uint16_t));
	SDL_GPUBufferCreateInfo vertexInfo {
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = vertexSize,
		.props = 0,
	};
	auto* vertexBuff = SDL_CreateGPUBuffer(_device, &vertexInfo);
	SDL_GPUBufferCreateInfo indexInfo {
		.usage = SDL_GPU_BUFFERUSAGE_INDEX,
		.size = indexSize,
		.props = 0
	};
	auto* indexBuff = SDL_CreateGPUBuffer(_device, &indexInfo);

	// big enough for both
	SDL_GPUTransferBufferCreateInfo transferInfo {
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = vertexSize + indexSize,
		.props = 0,
	};
	auto* transferBuff = SDL_CreateGPUTransferBuffer(_device, &transferInfo);
	auto* data = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(_device, transferBuff, false));
	if (!data) {
		SDL_Log("Failed to map TransferBuffer: %s", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(_device, transferBuff);
		return;
	}
	SDL_memcpy(data, vertices.data(), vertexSize);
	SDL_UnmapGPUTransferBuffer(_device, transferBuff);

	auto* cmdBuff = SDL_AcquireGPUCommandBuffer(_device);
	auto* copyPass = SDL_BeginGPUCopyPass(cmdBuff);

	// upload vertices
	SDL_GPUTransferBufferLocation vertexSrc {
		.transfer_buffer = transferBuff,
		.offset = 0,
	};
	SDL_GPUBufferRegion vertexDst {
		.buffer = vertexBuff,
		.offset = 0,
		.size = vertexSize,
	};
	SDL_UploadToGPUBuffer(copyPass, &vertexSrc, &vertexDst, false);

	// upload indices
	SDL_GPUTransferBufferLocation indexSrc {
		.transfer_buffer = transferBuff,
		.offset = vertexSize,
	};
	SDL_GPUBufferRegion indexDst {
		.buffer = indexBuff,
		.offset = 0,
		.size = indexSize
	};
	SDL_UploadToGPUBuffer(copyPass, &indexSrc, &indexDst, false);


	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(cmdBuff);
	SDL_ReleaseGPUTransferBuffer(_device, transferBuff);

	_vertexBuff = vertexBuff;
	_indexBuff = indexBuff;
}

SDL_Window* App::get_window() const {
	return _window;
}

void App::set_window(SDL_Window* window) {
	_window = window;
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
	SDL_ReleaseGPUBuffer(_device, _vertexBuff);
	SDL_ReleaseGPUBuffer(_device, _indexBuff);
	SDL_ReleaseWindowFromGPUDevice(_device, _window);
	SDL_DestroyGPUDevice(_device);
	SDL_DestroyWindow(_window);
}

