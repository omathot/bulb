module;
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <SDL3_shadercross/SDL_shadercross.h>

module app;
import std;

[[nodiscard]] static std::vector<char> read_file(const std::string& filename);
constexpr std::uint32_t make_vk_version(uint32_t major, uint32_t minor, uint32_t patch);

App::App() {
	if (enableDebug)
		SDL_Log("Starting app in Debug mode");
	else
		SDL_Log("Starting app in release mode");

	if (!SDL_ShaderCross_Init()) {
		SDL_Log("Failed to init ShaderCross: %s", SDL_GetError());
		exit(SDL_APP_FAILURE);
	}
	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Log("Video driver: %s", SDL_GetCurrentVideoDriver());

	auto* window = SDL_CreateWindow("Test", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
	if (!window) {
		SDL_Log("Failed to create window: %s", SDL_GetError());
	}
	set_window(window);

	SDL_GPUVulkanOptions vulkanOpts {
		.vulkan_api_version = make_vk_version(1, 3, 0),
	};
	SDL_PropertiesID gpuProps = SDL_CreateProperties();
	SDL_SetBooleanProperty(gpuProps, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
	SDL_SetBooleanProperty(gpuProps, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, enableDebug);
	SDL_SetStringProperty(gpuProps, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
	SDL_SetPointerProperty(gpuProps, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER, &vulkanOpts);
	auto* device = SDL_CreateGPUDeviceWithProperties(gpuProps);
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
	// shader
	auto fragCode = read_file(FRAGMENT_SHADER_PATH);
	auto vertCode = read_file(VERTEX_SHADER_PATH);
	SDL_GPUShaderCreateInfo vertInfo {
		.code_size = vertCode.size() * sizeof(char),
		.code = reinterpret_cast<const std::uint8_t*>(vertCode.data()),
		.entrypoint = "vertMain",
		.format = SDL_GPU_SHADERFORMAT_SPIRV,
		.stage = SDL_GPU_SHADERSTAGE_VERTEX,
		.num_samplers = 0,
		.num_storage_textures = 0,
		.num_storage_buffers = 0,
		.num_uniform_buffers = 1,
	};
	SDL_GPUShaderCreateInfo fragInfo {
		.code_size = fragCode.size() * sizeof(char),
		.code = reinterpret_cast<const std::uint8_t*>(fragCode.data()),
		.entrypoint = "fragMain",
		.format = SDL_GPU_SHADERFORMAT_SPIRV,
		.stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
		.num_samplers = 0,
		.num_storage_textures = 0,
		.num_storage_buffers = 0,
		.num_uniform_buffers = 0,
	};
	SDL_GPUShader* vertShader = SDL_CreateGPUShader(_device, &vertInfo);
	if (!vertShader) {
		SDL_Log("Failed to cross compile vertShader: %s", SDL_GetError());
	}
	SDL_GPUShader* fragShader = SDL_CreateGPUShader(_device, &fragInfo);
	if (!fragShader) {
		SDL_Log("Failed to cross compile vertShader: %s", SDL_GetError());
	}

	// pipeline
	SDL_GPUVertexBufferDescription vertexBuffDesc {
		.slot = 0,
		.pitch = sizeof(Vertex),
		.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
	};
	std::array<SDL_GPUVertexAttribute, 2> vertexAttributes = {
		{
			{
				.location = 0,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.offset = 0 // first member
			},
			{
				.location = 1,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = sizeof(glm::vec2) // comes after pos (glm vec2)
			}
		}
	};

	SDL_GPUVertexInputState vertexInput {
		.vertex_buffer_descriptions = &vertexBuffDesc,
		.num_vertex_buffers = 1,
		.vertex_attributes = static_cast<const SDL_GPUVertexAttribute*>(vertexAttributes.data()),
		.num_vertex_attributes = 2,
	};
	SDL_GPUColorTargetDescription colorTarget {
		.format = SDL_GetGPUSwapchainTextureFormat(_device, _window),
	};

	SDL_GPUGraphicsPipelineCreateInfo pipelineInfo {
		.vertex_shader = vertShader,
		.fragment_shader = fragShader,
		.vertex_input_state = vertexInput,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.target_info = {
			.color_target_descriptions = &colorTarget,
			.num_color_targets = 1,
		},
	};

	_pipeline = SDL_CreateGPUGraphicsPipeline(_device, &pipelineInfo);
	SDL_ReleaseGPUShader(_device, fragShader);
	SDL_ReleaseGPUShader(_device, vertShader);

	// buffers
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
	SDL_memcpy(data + vertexSize, indices.data(), indexSize);
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

SDL_GPUBuffer* App::get_vertex_buff() const {
	return _vertexBuff;
}

SDL_GPUBuffer* App::get_index_buff() const {
	return _indexBuff;
}

SDL_GPUGraphicsPipeline* App::get_graphics_pipeline() const {
	return _pipeline;
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
	SDL_ReleaseGPUGraphicsPipeline(_device, _pipeline);
	SDL_ReleaseWindowFromGPUDevice(_device, _window);
	SDL_DestroyGPUDevice(_device);
	SDL_DestroyWindow(_window);

	SDL_Quit();
}

[[nodiscard]] static std::vector<char> read_file(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("failed to open file");

	std::vector<char> buffer(static_cast<unsigned long>(file.tellg()));
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
	file.close();
	return buffer;
}

constexpr std::uint32_t make_vk_version(uint32_t major, uint32_t minor, uint32_t patch) {
	return (major << 22) | (minor << 12) | patch;
}
