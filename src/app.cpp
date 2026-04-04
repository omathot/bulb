module;
#include <SDL3/SDL.h>

module app;
import std;

App::App() {
	if (enableDebug)
		SDL_Log("Starting app in Debug mode");
	else
		SDL_Log("Starting app in release mode");

	SDL_Init(SDL_INIT_VIDEO);
	set_window(SDL_CreateWindow("Test", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE));
	set_device(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, enableDebug, "vulkan")); // prefer vulkan
	// SDL_ClaimWindowForGPUDevice(this->_device, this->_window);
	set_renderer(SDL_CreateRenderer(get_window(), nullptr));
}

SDL_Window* App::get_window() const {
	return this->_window;
}

void App::set_window(SDL_Window* window) {
	this->_window = window;
}

SDL_Renderer* App::get_renderer() const {
	return this->_renderer;
}

void App::set_renderer(SDL_Renderer* renderer) {
	this->_renderer = renderer;
}

SDL_GPUDevice* App::get_device() const {
	return this->_device;
}

void App::set_device(SDL_GPUDevice* device) {
	this->_device = device;
}

void App::terminate() {
	this->_should_exit = true;
}

bool App::should_exit() const {
	return this->_should_exit;
}

void App::cleanup() {
	SDL_DestroyRenderer(this->_renderer);
	SDL_DestroyGPUDevice(this->_device);
	SDL_DestroyWindow(this->_window);
}

