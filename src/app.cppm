module;
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

export module app;
import std;

const std::string SHADER_PATH = "/home/omathot/dev/cpp/sdl3/shaders/shader.spv";

constexpr std::uint32_t WINDOW_WIDTH = 800;
constexpr std::uint32_t WINDOW_HEIGHT = 600;

#if defined(NDEBUG)
	constexpr bool enableDebug = false;
#else
	constexpr bool enableDebug = true;
#endif


struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;
};

const std::vector<Vertex> vertices = {
    {.pos={-0.5f, -0.5f}, .color={1.0f, 0.0f, 0.0f}},
    {.pos={0.5f, -0.5f}, .color={0.0f, 1.0f, 0.0f}},
    {.pos={0.5f, 0.5f}, .color={0.0f, 0.0f, 1.0f}},
    {.pos={-0.5f, 0.5f}, .color={1.0f, 1.0f, 1.0f}}
};
const std::vector<std::uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};



export class App {
public:
	App();

	void cleanup();
	[[nodiscard]] SDL_Window* get_window() const;
	void set_window(SDL_Window* window);
	[[nodiscard]] SDL_GPUDevice* get_device() const;
	void set_device(SDL_GPUDevice* device);
	void terminate();
	[[nodiscard]] bool should_exit() const;
	[[nodiscard]] SDL_GPUBuffer* get_vertex_buff() const;
	[[nodiscard]] SDL_GPUBuffer* get_index_buff() const;

private:
	SDL_Window* _window = nullptr;
	SDL_GPUDevice* _device = nullptr;
	bool _should_exit = false;

	SDL_GPUBuffer* _vertexBuff = nullptr;
	SDL_GPUBuffer* _indexBuff = nullptr;

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
