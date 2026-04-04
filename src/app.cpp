module;
#include <SDL3/SDL.h>

module app;
import std;

App::App() : _window(nullptr), _should_close(false) {
}

SDL_Window* App::get_window() const {
	return this->_window;
}

void App::set_window(SDL_Window* window) {
	this->_window = window;
}

void App::should_exit() {
	this->_should_close = true;
}

void App::run() {
	while (!this->_should_close) {

	}
}

void App::cleanup() {

}



