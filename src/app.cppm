module;
#include <SDL3/SDL.h>

export module app;
import std;

constexpr std::uint32_t WINDOW_WIDTH = 800;
constexpr std::uint32_t WINDOW_HEIGHT = 600;

#if defined(NDEBUG)
	constexpr bool enableDebug = false;
#else
	constexpr bool enableDebug = true;
#endif

export class App {
public:
	App();

	void cleanup();
	[[nodiscard]] SDL_Window* get_window() const;
	void set_window(SDL_Window* window);
	[[nodiscard]] SDL_Renderer* get_renderer() const;
	void set_renderer(SDL_Renderer* renderer);
	[[nodiscard]] SDL_GPUDevice* get_device() const;
	void set_device(SDL_GPUDevice* device);
	void terminate();
	[[nodiscard]] bool should_exit() const;

private:
	SDL_Window* _window = nullptr;
	SDL_Renderer* _renderer = nullptr;
	SDL_GPUDevice* _device = nullptr;
	bool _should_exit = false;
};

// suppressing system/driver leaks
extern "C" const char* __lsan_default_suppressions() { // NOLINT
	return
		"leak:SDL_CreateGPUDevice\n"
		"leak:SDL_CreateRenderer\n"
		"leak:asan_thread_start\n";
}
