module;
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <SDL3/SDL.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONEG
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <tiny_obj_loader.h>

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
	_texture_manager = std::make_unique<TextureManager>(_device);

	SDL_ClaimWindowForGPUDevice(_device, _window);
	setup_gpu_resources();
	_camera = std::make_unique<Camera>();
}

void App::setup_gpu_resources() {
	_texture_manager->load(TITANIC_TEXTURE);
	load_model();
	// sampler
	SDL_GPUSamplerCreateInfo sampler_info {
		.min_filter = SDL_GPU_FILTER_LINEAR,
		.mag_filter = SDL_GPU_FILTER_LINEAR,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT
	};
	_sampler = SDL_CreateGPUSampler(_device, &sampler_info);

	// depth
	SDL_GPUTextureCreateInfo depth_info {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
		.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
		.width = WINDOW_WIDTH,
		.height = WINDOW_HEIGHT,
		.layer_count_or_depth = 1,
		.num_levels = 1,
	};
	_depth_texture = SDL_CreateGPUTexture(_device, &depth_info);

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
		.num_uniform_buffers = 1,
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
	// ! IMPORTANT: If adding, don't forget to bump num_vertex_attributes below!
	std::array<SDL_GPUVertexAttribute, 4> vertex_attributes = {
		{
			{ // pos
				.location = 0,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = 0
			},
			{ // color
				.location = 1,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = sizeof(glm::vec3)
			},
			{ // uv
				.location = 2,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.offset = sizeof(glm::vec3) * 2,
			},
			{ // normal
				.location = 3,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = (sizeof(glm::vec3) * 2) + sizeof(glm::vec2),
			}
		}
	};

	SDL_GPUVertexInputState vertex_input {
		.vertex_buffer_descriptions = &vertex_buff_desc,
		.num_vertex_buffers = 1,
		.vertex_attributes = static_cast<const SDL_GPUVertexAttribute*>(vertex_attributes.data()),
		.num_vertex_attributes = 4,
	};
	SDL_GPUColorTargetDescription color_target {
		.format = SDL_GetGPUSwapchainTextureFormat(_device, _window),
	};

	SDL_GPUGraphicsPipelineCreateInfo graphics_pipeline_info {
		.vertex_shader = vert_shader,
		.fragment_shader = frag_shader,
		.vertex_input_state = vertex_input,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.depth_stencil_state = {
			.compare_op = SDL_GPU_COMPAREOP_LESS,
			.enable_depth_test = true,
			.enable_depth_write = true,
		},
		.target_info = {
			.color_target_descriptions = &color_target,
			.num_color_targets = 1,
			.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
			.has_depth_stencil_target = true,
		},
	};
	SDL_GPUGraphicsPipelineCreateInfo wireframe_pipeline_info {
		.vertex_shader = vert_shader,
		.fragment_shader = frag_shader,
		.vertex_input_state = vertex_input,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.rasterizer_state = {
			.fill_mode = SDL_GPU_FILLMODE_LINE,
		},
		.depth_stencil_state = {
			.compare_op = SDL_GPU_COMPAREOP_LESS,
			.enable_depth_test = true,
			.enable_depth_write = true,
		},
		.target_info = {
			.color_target_descriptions = &color_target,
			.num_color_targets = 1,
			.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
			.has_depth_stencil_target = true,
		},
	};

	_graphics_pipeline = SDL_CreateGPUGraphicsPipeline(_device, &graphics_pipeline_info);
	_wireframe_pipeline = SDL_CreateGPUGraphicsPipeline(_device, &wireframe_pipeline_info);
	SDL_ReleaseGPUShader(_device, frag_shader);
	SDL_ReleaseGPUShader(_device, vert_shader);

	// buffers
	std::uint32_t vertex_size = static_cast<Uint32>(_vertices.size() * sizeof(Vertex));
	std::uint32_t index_size = static_cast<Uint32>(_indices.size() * sizeof(std::uint32_t));
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
	SDL_GPUTransferBufferCreateInfo transfer_info {
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = vertex_size + index_size,
		.props = 0,
	};
	auto* transfer_buff = SDL_CreateGPUTransferBuffer(_device, &transfer_info);
	auto* data = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(_device, transfer_buff, false));
	if (!data) {
		SDL_Log("Failed to map TransferBuffer: %s", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(_device, transfer_buff);
		return;
	}
	SDL_memcpy(data, _vertices.data(), vertex_size);
	SDL_memcpy(data + vertex_size, _indices.data(), index_size);
	SDL_UnmapGPUTransferBuffer(_device, transfer_buff);

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

	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(cmd_buff);
	SDL_ReleaseGPUTransferBuffer(_device, transfer_buff);

	_vertex_buff = vertex_buff;
	_index_buff = index_buff;
}

SDL_AppResult App::iterate() {
	if (should_exit())
		return SDL_APP_SUCCESS;

	// get swapchain
	auto* cmd_buff = SDL_AcquireGPUCommandBuffer(_device);
	SDL_GPUTexture* swapchain = nullptr;
	SDL_WaitAndAcquireGPUSwapchainTexture(cmd_buff, _window, &swapchain, nullptr, nullptr);
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
	SDL_GPUDepthStencilTargetInfo depth_target {
		.texture = _depth_texture,
		.clear_depth = 1.0f,
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_DONT_CARE,
	};
	auto* render_pass = SDL_BeginGPURenderPass(cmd_buff, &color_info, 1, &depth_target);
	if (_wireframe)
		SDL_BindGPUGraphicsPipeline(render_pass, _wireframe_pipeline);
	else
		SDL_BindGPUGraphicsPipeline(render_pass, _graphics_pipeline);
	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(_window, &w, &h);
	// SDL defaults to full, just like being explicit
	SDL_GPUViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.w = static_cast<float>(w),
		.h = static_cast<float>(h),
		.min_depth = 0.0f,
		.max_depth = 1.0f,
	};
	SDL_SetGPUViewport(render_pass, &viewport);

	// push constants
	static auto start_time = std::chrono::high_resolution_clock::now();
	auto  current_time = std::chrono::high_resolution_clock::now();
	_dt = std::chrono::duration<float>(current_time - start_time).count();
	start_time = current_time;

	if (_camera->move_forwards)
		_camera->_pos += _camera->front() * _camera->_speed * _dt;
	if (_camera->move_backwards)
		_camera->_pos -= _camera->front() * _camera->_speed * _dt;
	if (_camera->move_left)
		_camera->_pos -= _camera->right() * _camera->_speed * _dt;
	if (_camera->move_right)
		_camera->_pos += _camera->right() * _camera->_speed * _dt;
	if (_camera->move_up)
		_camera->_pos.y += _camera->_speed * _dt;
	if (_camera->move_down)
		_camera->_pos.y -= _camera->_speed * _dt;
	auto* tex = _texture_manager->get_tex();
	if (!tex->_controller.pause)
	    tex->_controller._angle += _dt * glm::radians(static_cast<float>(tex->_controller._rot_speed));

	float aspect = static_cast<float>(w) / static_cast<float>(h);
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 300.0f); // last arg is where depth will clamp and stop rendering past that distance
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model, tex->_controller._angle, glm::vec3(0.0f, 1.0f, 0.0f));
	UniformBuffer ubo {
		.model = model,
		.view = _camera->view_matrix(),
		.proj = proj
	};
	SDL_PushGPUVertexUniformData(cmd_buff, 0, &ubo, sizeof(ubo));
	LightBuffer light_data {
		.light_dir = glm::normalize(glm::vec3(-1.0f, -1.0f, -0.5f)),
		.light_color = glm::vec3(1.0f),
		.camera_pos = _camera->_pos,
		.enabled = _lighting_enabled ? 1.0f : 0.0f,
	};
	SDL_PushGPUFragmentUniformData(cmd_buff, 0, &light_data, sizeof(light_data));

	// bindings
	SDL_GPUBufferBinding index_binding {
		.buffer = _index_buff,
		.offset = 0,
	};
	SDL_GPUBufferBinding vertex_binding {
		.buffer = _vertex_buff,
		.offset = 0,
	};
	SDL_GPUTextureSamplerBinding sampler_binding {
		.texture = _texture_manager->get_tex()->_data,
		.sampler = _sampler,
	};
	if (_wireframe)
		SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
	SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
	if (_wireframe)
		SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);
	SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);
	if (_wireframe)
		SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);
	SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

	// draw
	SDL_DrawGPUIndexedPrimitives(render_pass, static_cast<std::uint32_t>(_indices.size()), 1, 0, 0, 0);
	SDL_EndGPURenderPass(render_pass);

	SDL_SubmitGPUCommandBuffer(cmd_buff);

	return SDL_APP_CONTINUE;
}

SDL_AppResult App::handle_event(SDL_Event* event) {
	switch (event->type) {
		case SDL_EVENT_QUIT:
			terminate();
			break;
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
				auto w = static_cast<std::uint32_t>(event->window.data1);
				auto h = static_cast<std::uint32_t>(event->window.data2);
				if (w == 0 || h == 0)
					break;
				SDL_ReleaseGPUTexture(_device, _depth_texture);
				SDL_GPUTextureCreateInfo depth_info {
					.type = SDL_GPU_TEXTURETYPE_2D,
					.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
					.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
					.width = w,
					.height = h,
					.layer_count_or_depth = 1,
					.num_levels = 1,
				};
				_depth_texture = SDL_CreateGPUTexture(_device, &depth_info);
				break;
		}
		case SDL_EVENT_MOUSE_MOTION:
			if (!_mouse_captured)
				break;
			_camera->_yaw += event->motion.xrel * _camera->_sensitivity;
			_camera->_pitch -= event->motion.yrel * _camera->_sensitivity;
			// clamp
			if (_camera->_pitch > 89.0f) // NOLINT
				_camera->_pitch = 89.0f;
			if (_camera->_pitch < -89.0f) // NOLINT
				_camera->_pitch = -89.0f;
			break;
		case SDL_EVENT_KEY_DOWN:
			switch (event->key.key) {
				case SDLK_ESCAPE:
					terminate();
					break;
				case SDLK_A:
					_camera->move_left = true;
					break;
				case SDLK_D:
					_camera->move_right = true;
					break;
				case SDLK_W:
					_camera->move_forwards = true;
					break;
				case SDLK_S:
					_camera->move_backwards = true;
					break;
				case SDLK_UP:
					_camera->move_up = true;
					break;
				case SDLK_DOWN:
					_camera->move_down = true;
					break;
				case SDLK_SPACE:
					_texture_manager->get_tex()->_controller.pause = !_texture_manager->get_tex()->_controller.pause;
					break;
				case SDLK_TAB:
					_mouse_captured = !_mouse_captured;
					SDL_SetWindowRelativeMouseMode(_window, _mouse_captured);
					break;
				case SDLK_L:
					_lighting_enabled = !_lighting_enabled;
					SDL_Log("Lighting enabled: %d", _lighting_enabled);
					break;
				case SDLK_Z:
					_wireframe = !_wireframe;
					break;
				default:
					SDL_Log("key down: %s", SDL_GetKeyName(event->key.key));
					break;
			}
			break;
		case SDL_EVENT_KEY_UP:
			switch (event->key.key) {
				case SDLK_A:
					_camera->move_left = false;
					break;
				case SDLK_D:
					_camera->move_right = false;
					break;
				case SDLK_W:
					_camera->move_forwards = false;
					break;
				case SDLK_S:
					_camera->move_backwards = false;
					break;
				case SDLK_UP:
					_camera->move_up = false;
					break;
				case SDLK_DOWN:
					_camera->move_down = false;
					break;
				default:
					break;
			}
			break;
		default: {
			std::array<char, 256> desc{};
			SDL_GetEventDescription(event, static_cast<char*>(desc.data()), sizeof(desc));
			SDL_Log("Unhandled event: %s", static_cast<char*>(desc.data()));
			break;
		}
	}
	return SDL_APP_CONTINUE;
}

void App::set_window(SDL_Window* window) {
	_window = window;
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
	_texture_manager->cleanup();
	SDL_ReleaseGPUTexture(_device, _depth_texture);
	SDL_ReleaseGPUBuffer(_device, _vertex_buff);
	SDL_ReleaseGPUBuffer(_device, _index_buff);
	SDL_ReleaseGPUSampler(_device, _sampler);
	SDL_ReleaseGPUGraphicsPipeline(_device, _graphics_pipeline);
	SDL_ReleaseGPUGraphicsPipeline(_device, _wireframe_pipeline);
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

void App::load_model() {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	std::string basedir = "/home/omathot/dev/cpp/bulb/assets/models/";

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, TITANIC_MODEL.c_str(), basedir.c_str())) {
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<Vertex, std::uint32_t> unique_vertices;
	for (const auto& shape: shapes) {
		for (const auto& idx: shape.mesh.indices) {
			Vertex vertex{};
			vertex.pos = {
				attrib.vertices[(3 * static_cast<ulong>(idx.vertex_index)) + 0],
				attrib.vertices[(3 * static_cast<ulong>(idx.vertex_index)) + 1],
				attrib.vertices[(3 * static_cast<ulong>(idx.vertex_index)) + 2]
			};
			if (idx.texcoord_index >= 0) {
				vertex.uv = {
					attrib.texcoords[(2 * static_cast<ulong>(idx.texcoord_index)) + 0],
					1.0f - attrib.texcoords[(2 * static_cast<ulong>(idx.texcoord_index)) + 1]
				};
			} else {
				vertex.uv = {0.0f, 0.0f};
			}
			vertex.color = {1.0f, 1.0f, 1.0f};

			if (idx.normal_index >= 0) {
				vertex.normal = {
					attrib.normals[(3 * static_cast<ulong>(idx.normal_index)) + 0],
					attrib.normals[(3 * static_cast<ulong>(idx.normal_index)) + 1],
					attrib.normals[(3 * static_cast<ulong>(idx.normal_index)) + 2],
				};
			} else {
				SDL_Log("Used fallback for normal");
				vertex.normal = {0.0f, 1.0f, 0.0f};
			}

			if (!unique_vertices.contains(vertex)) {
				unique_vertices[vertex] = static_cast<std::uint32_t>(_vertices.size());
				_vertices.push_back(vertex);
			}
			// if already seen index to it with vertex
			_indices.push_back(unique_vertices[vertex]);
		}
	}
	std::cout << "Successfully loaded model, uniquevertices = " << _vertices.size() << '\n';
}
