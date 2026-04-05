module;
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

export module app;
import std;

const std::string FRAGMENT_SHADER_PATH = "/home/omathot/dev/cpp/bulb/shaders/fragment.spv";
const std::string VERTEX_SHADER_PATH = "/home/omathot/dev/cpp/bulb/shaders/vertex.spv";
const std::string TEXTURE_PATH = "/home/omathot/dev/cpp/bulb/assets/fern_red.png";

constexpr std::uint32_t WINDOW_WIDTH = 800;
constexpr std::uint32_t WINDOW_HEIGHT = 600;

#if defined(NDEBUG)
	constexpr bool enable_debug = false;
#else
	constexpr bool enable_debug = true;
#endif


struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 uv;
};

// Y up
// "Ground" is XZ
const std::vector<Vertex> vertices = {
    {.pos={-0.5f, 0.0f, -0.5f}, .color={1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}},
    {.pos={0.5f, 0.0f, -0.5f}, .color={0.0f, 1.0f, 0.0f}, .uv = {0.0f, 0.0f,}},
    {.pos={0.5f, 0.0f, 0.5f}, .color={0.0f, 0.0f, 1.0f}, .uv = {0.0f, 1.0f,}},
    {.pos={-0.5f, 0.0f, 0.5f}, .color={1.0f, 1.0f, 1.0}, .uv = {1.0f, 1.0f}}
};
const std::vector<std::uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

struct UniformBuffer {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct Controller {
	glm::vec3 _pos{{}};
	float _angle = 0;
	std::uint32_t _speed = 7;
	bool move_left = false;
	bool move_right = false;
	bool move_further = false;
	bool move_closer = false;
	bool move_up = false;
	bool move_down = false;
};

struct Texture {
	SDL_GPUTexture* _data;
	Controller _controller{};

	Texture(SDL_GPUTexture* texture) : _data(texture) {}
};

export class TextureManager {
public:
	explicit TextureManager(SDL_GPUDevice* device);
	Texture* load(const std::string& path);
	[[nodiscard]] Texture* get_tex() const;
	void cleanup();

private:
	SDL_GPUDevice* _device;
	std::vector<std::unique_ptr<Texture>> _textures;
};


export class App {
public:
	App();

	[[nodiscard]] SDL_AppResult iterate();
	[[nodiscard]] SDL_AppResult handle_event(SDL_Event* event);
	void cleanup();

private:
	SDL_Window* _window = nullptr;
	SDL_GPUDevice* _device = nullptr;

	SDL_GPUBuffer* _vertex_buff = nullptr;
	SDL_GPUBuffer* _index_buff = nullptr;
	std::unique_ptr<TextureManager> _texture_manager;
	// SDL_GPUTexture* _texture = nullptr;

	SDL_GPUGraphicsPipeline* _graphics_pipeline = nullptr;
	SDL_GPUSampler* _sampler = nullptr;

	float _dt{};
	bool _should_exit = false;

	void set_window(SDL_Window* window);
	void set_device(SDL_GPUDevice* device);
	void setup_gpu_resources();
	void terminate();
	[[nodiscard]] bool should_exit() const;
};

// suppressing system/driver leaks
extern "C" const char* __lsan_default_suppressions() { // NOLINT
	return
		"leak:SDL_CreateGPUDevice\n"
		"leak:asan_thread_start\n"
		"leak:X11_\n"
		"leak:libxkbcommon\n";
}
