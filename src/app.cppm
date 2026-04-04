module;
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

export module app;
import std;

const std::string FRAGMENT_SHADER_PATH = "/home/omathot/dev/cpp/bulb/shaders/fragment.spv";
const std::string VERTEX_SHADER_PATH = "/home/omathot/dev/cpp/bulb/shaders/vertex.spv";

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
};

const std::vector<Vertex> vertices = {
    {.pos={-0.5f, -0.5f, 0.0f,}, .color={1.0f, 0.0f, 0.0f}},
    {.pos={0.5f, -0.5f, 0.0f,}, .color={0.0f, 1.0f, 0.0f}},
    {.pos={0.5f, 0.5f, 0.0f,}, .color={0.0f, 0.0f, 1.0f}},
    {.pos={-0.5f, 0.5f, 0.0f,}, .color={1.0f, 1.0f, 1.0f}}
};
const std::vector<std::uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

struct UniformBuffer {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};


export class App {
public:
	App();

	void cleanup();
	[[nodiscard]] SDL_AppResult iterate() const;
	[[nodiscard]] SDL_AppResult handle_event(SDL_Event* event);
	[[nodiscard]] SDL_Window* get_window() const;
	void set_window(SDL_Window* window);
	[[nodiscard]] SDL_GPUDevice* get_device() const;
	void set_device(SDL_GPUDevice* device);
	void terminate();
	[[nodiscard]] bool should_exit() const;
	[[nodiscard]] SDL_GPUBuffer* get_vertex_buff() const;
	[[nodiscard]] SDL_GPUBuffer* get_index_buff() const;
	[[nodiscard]] SDL_GPUGraphicsPipeline* get_graphics_pipeline() const;

private:
	SDL_Window* _window = nullptr;
	SDL_GPUDevice* _device = nullptr;
	bool _should_exit = false;

	SDL_GPUBuffer* _vertex_buff = nullptr;
	SDL_GPUBuffer* _index_buff = nullptr;

	SDL_GPUGraphicsPipeline* _graphics_pipeline = nullptr;

	void setup_gpu_resources();
};

// suppressing system/driver leaks
extern "C" const char* __lsan_default_suppressions() { // NOLINT
	return
		"leak:SDL_CreateGPUDevice\n"
		"leak:asan_thread_start\n"
		"leak:X11_\n"
		"leak:libxkbcommon\n";
}
