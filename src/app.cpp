module;
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <SDL3/SDL.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONEG
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3_shadercross/SDL_shadercross.h>

module app;
import std;

[[nodiscard]] static std::vector<char> read_file(const std::string& filename);
[[nodiscard]] constexpr std::uint32_t make_vk_version(uint32_t major, uint32_t minor, uint32_t patch);

App::App() {
	if (enable_debug)
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

	SDL_GPUVulkanOptions vulkan_opts {
		.vulkan_api_version = make_vk_version(1, 3, 0),
	};
	SDL_PropertiesID gpu_props = SDL_CreateProperties();
	SDL_SetBooleanProperty(gpu_props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
	SDL_SetBooleanProperty(gpu_props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, enable_debug);
	SDL_SetStringProperty(gpu_props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
	SDL_SetPointerProperty(gpu_props, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER, &vulkan_opts);
	auto* device = SDL_CreateGPUDeviceWithProperties(gpu_props);
	if (!device) {
		SDL_Log("Failed to create GPU Device: %s", SDL_GetError());
	}
	set_device(device); // prefer vulkan
	SDL_PropertiesID device_props = SDL_GetGPUDeviceProperties(_device);
	const auto* name = SDL_GetStringProperty(device_props, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown");
	SDL_Log("Picked device: %s", name);
	SDL_Log("using driver: %s", SDL_GetGPUDeviceDriver(_device));

	SDL_ClaimWindowForGPUDevice(_device, _window);
	setup_gpu_resources();
}

void App::setup_gpu_resources() {
	// sampler
	SDL_GPUSamplerCreateInfo sampler_info {
		.min_filter = SDL_GPU_FILTER_LINEAR,
		.mag_filter = SDL_GPU_FILTER_LINEAR,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT
	};
	_sampler = SDL_CreateGPUSampler(_device, &sampler_info);
	// shader
	auto frag_code = read_file(FRAGMENT_SHADER_PATH);
	auto vert_code = read_file(VERTEX_SHADER_PATH);
	SDL_GPUShaderCreateInfo vert_info {
		.code_size = vert_code.size() * sizeof(char),
		.code = reinterpret_cast<const std::uint8_t*>(vert_code.data()),
		.entrypoint = "vertMain",
		.format = SDL_GPU_SHADERFORMAT_SPIRV,
		.stage = SDL_GPU_SHADERSTAGE_VERTEX,
		.num_samplers = 0,
		.num_storage_textures = 0,
		.num_storage_buffers = 0,
		.num_uniform_buffers = 1,
	};
	SDL_GPUShaderCreateInfo frag_info {
		.code_size = frag_code.size() * sizeof(char),
		.code = reinterpret_cast<const std::uint8_t*>(frag_code.data()),
		.entrypoint = "fragMain",
		.format = SDL_GPU_SHADERFORMAT_SPIRV,
		.stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
		.num_samplers = 1,
		.num_storage_textures = 0,
		.num_storage_buffers = 0,
		.num_uniform_buffers = 0,
	};
	SDL_GPUShader* vert_shader = SDL_CreateGPUShader(_device, &vert_info);
	if (!vert_shader) {
		SDL_Log("Failed to create vertShader: %s", SDL_GetError());
	}
	SDL_GPUShader* frag_shader = SDL_CreateGPUShader(_device, &frag_info);
	if (!frag_shader) {
		SDL_Log("Failed to create vertShader: %s", SDL_GetError());
	}

	// pipeline
	SDL_GPUVertexBufferDescription vertex_buff_desc {
		.slot = 0,
		.pitch = sizeof(Vertex),
		.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
	};
	std::array<SDL_GPUVertexAttribute, 3> vertex_attributes = {
		{
			{ // pos
				.location = 0,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = 0 // first member
			},
			{ // color
				.location = 1,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = sizeof(glm::vec3) // comes after pos (glm vec3)
			},
			{ // uv
				.location = 2,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.offset = sizeof(glm::vec3) + sizeof(glm::vec3),
			}
		}
	};

	SDL_GPUVertexInputState vertex_input {
		.vertex_buffer_descriptions = &vertex_buff_desc,
		.num_vertex_buffers = 1,
		.vertex_attributes = static_cast<const SDL_GPUVertexAttribute*>(vertex_attributes.data()),
		.num_vertex_attributes = 3,
	};
	SDL_GPUColorTargetDescription color_target {
		.format = SDL_GetGPUSwapchainTextureFormat(_device, _window),
	};

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info {
		.vertex_shader = vert_shader,
		.fragment_shader = frag_shader,
		.vertex_input_state = vertex_input,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.target_info = {
			.color_target_descriptions = &color_target,
			.num_color_targets = 1,
		},
	};

	_graphics_pipeline = SDL_CreateGPUGraphicsPipeline(_device, &pipeline_info);
	SDL_ReleaseGPUShader(_device, frag_shader);
	SDL_ReleaseGPUShader(_device, vert_shader);

	// buffers
	std::uint32_t vertex_size = static_cast<Uint32>(vertices.size() * sizeof(Vertex));
	std::uint32_t index_size = static_cast<Uint32>(indices.size() * sizeof(std::uint16_t));
	SDL_GPUBufferCreateInfo vertexInfo {
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = vertex_size,
		.props = 0,
	};
	auto* vertex_buff = SDL_CreateGPUBuffer(_device, &vertexInfo);
	SDL_GPUBufferCreateInfo index_info {
		.usage = SDL_GPU_BUFFERUSAGE_INDEX,
		.size = index_size,
		.props = 0
	};
	auto* index_buff = SDL_CreateGPUBuffer(_device, &index_info);

	// texture
	int tex_width = 0, tex_height = 0, tex_channels = 0;
	stbi_uc* pixels{};
	pixels = stbi_load(TEXTURE_PATH.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
	if (!pixels) {
		SDL_Log("Failed to get pixels from texture");
		exit(1);
	}
	std::uint32_t tex_size = static_cast<std::uint32_t>(tex_height) * static_cast<std::uint32_t>(tex_width) * 4;
	SDL_GPUTextureCreateInfo tex_info {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
		.width = static_cast<std::uint32_t>(tex_width),
		.height = static_cast<std::uint32_t>(tex_height),
		.layer_count_or_depth = 1,
		.num_levels = 1,
	};
	_texture = SDL_CreateGPUTexture(_device, &tex_info);

	// big enough for both buffs + texture
	SDL_GPUTransferBufferCreateInfo transfer_info {
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = vertex_size + index_size + tex_size,
		.props = 0,
	};
	auto* transfer_buff = SDL_CreateGPUTransferBuffer(_device, &transfer_info);
	auto* data = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(_device, transfer_buff, false));
	if (!data) {
		SDL_Log("Failed to map TransferBuffer: %s", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(_device, transfer_buff);
		return;
	}
	SDL_memcpy(data, vertices.data(), vertex_size);
	SDL_memcpy(data + vertex_size, indices.data(), index_size);
	SDL_memcpy(data + vertex_size + index_size, pixels, tex_size);
	SDL_UnmapGPUTransferBuffer(_device, transfer_buff);
	stbi_image_free(pixels);

	auto* cmd_buff = SDL_AcquireGPUCommandBuffer(_device);
	auto* copy_pass = SDL_BeginGPUCopyPass(cmd_buff);

	// upload vertices
	SDL_GPUTransferBufferLocation vertex_src {
		.transfer_buffer = transfer_buff,
		.offset = 0,
	};
	SDL_GPUBufferRegion vertex_dst {
		.buffer = vertex_buff,
		.offset = 0,
		.size = vertex_size,
	};
	SDL_UploadToGPUBuffer(copy_pass, &vertex_src, &vertex_dst, false);

	// upload indices
	SDL_GPUTransferBufferLocation index_src {
		.transfer_buffer = transfer_buff,
		.offset = vertex_size,
	};
	SDL_GPUBufferRegion index_dst {
		.buffer = index_buff,
		.offset = 0,
		.size = index_size
	};
	SDL_UploadToGPUBuffer(copy_pass, &index_src, &index_dst, false);

	// upload texture
	SDL_GPUTextureTransferInfo tex_src {
		.transfer_buffer = transfer_buff,
		.offset = vertex_size + index_size,
	};
	SDL_GPUTextureRegion tex_dst {
		.texture = _texture,
		.w = static_cast<std::uint32_t>(tex_width),
		.h = static_cast<std::uint32_t>(tex_height),
		.d = 1,
	};
	SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);


	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(cmd_buff);
	SDL_ReleaseGPUTransferBuffer(_device, transfer_buff);

	_vertex_buff = vertex_buff;
	_index_buff = index_buff;
}

SDL_AppResult App::iterate() const {
	if (should_exit())
		return SDL_APP_SUCCESS;

	auto* cmd_buff = SDL_AcquireGPUCommandBuffer(get_device());
	SDL_GPUTexture* swapchain = nullptr;
	SDL_WaitAndAcquireGPUSwapchainTexture(cmd_buff, get_window(), &swapchain, nullptr, nullptr);
	if (!swapchain) {
		SDL_SubmitGPUCommandBuffer(cmd_buff);
		return SDL_APP_CONTINUE;
	}
	SDL_GPUColorTargetInfo color_info {
		.texture = swapchain,
		.clear_color = {.r = 0.1f, .g=0.01f, .b=0.1f, .a = 1.0f},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE,
	};
	auto* render_pass = SDL_BeginGPURenderPass(cmd_buff, &color_info, 1, nullptr);
	SDL_BindGPUGraphicsPipeline(render_pass, get_graphics_pipeline());


	// push constants
	static auto start_time = std::chrono::high_resolution_clock::now();
	auto  current_time = std::chrono::high_resolution_clock::now();
	float time        = std::chrono::duration<float>(current_time - start_time).count();

	float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
	float angle = time * glm::radians(90.0f);
	UniformBuffer ubo {
		.model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)),
		.view = glm::lookAt(
			glm::vec3(2.0f, 2.0f, 2.0f),
			glm::vec3(0.0f, 0.0, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
		),
		.proj = proj
	};
	SDL_PushGPUVertexUniformData(cmd_buff, 0, &ubo, sizeof(ubo));

	// bindings
	SDL_GPUBufferBinding index_binding {
		.buffer = get_index_buff(),
		.offset = 0,
	};
	SDL_GPUBufferBinding vertex_binding {
		.buffer = get_vertex_buff(),
		.offset = 0,
	};
	SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
	SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);
	SDL_GPUTextureSamplerBinding sampler_binding {
		.texture = _texture,
		.sampler = _sampler,
	};
	SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

	// draw
	SDL_DrawGPUIndexedPrimitives(render_pass, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
	SDL_EndGPURenderPass(render_pass);

	SDL_SubmitGPUCommandBuffer(cmd_buff);

	return SDL_APP_CONTINUE;
}

SDL_AppResult App::handle_event(SDL_Event* event) {
	switch (event->type) {
		case SDL_EVENT_QUIT:
			terminate();
			break;
		case SDL_EVENT_KEY_DOWN:
			switch (event->key.key) {
				case SDLK_ESCAPE:
					terminate();
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
			std::array<char, 256> desc{};
			SDL_GetEventDescription(event, static_cast<char*>(desc.data()), sizeof(desc));
			SDL_Log("Unhandled event: %s", static_cast<char*>(desc.data()));
			break;
	}
	return SDL_APP_CONTINUE;
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
	return _vertex_buff;
}

SDL_GPUBuffer* App::get_index_buff() const {
	return _index_buff;
}

SDL_GPUGraphicsPipeline* App::get_graphics_pipeline() const {
	return _graphics_pipeline;
}

void App::terminate() {
	_should_exit = true;
}

bool App::should_exit() const {
	return _should_exit;
}

void App::cleanup() {
	SDL_ReleaseGPUTexture(_device, _texture);
	SDL_ReleaseGPUBuffer(_device, _vertex_buff);
	SDL_ReleaseGPUBuffer(_device, _index_buff);
	SDL_ReleaseGPUSampler(_device, _sampler);
	SDL_ReleaseGPUGraphicsPipeline(_device, _graphics_pipeline);
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

[[nodiscard]] constexpr std::uint32_t make_vk_version(uint32_t major, uint32_t minor, uint32_t patch) {
	return (major << 22) | (minor << 12) | patch;
}
