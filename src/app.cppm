module;
#include <SDL3/SDL.h>

export module app;
import std;

export class App {
public:
	App();

	void run();
	void cleanup();
	[[nodiscard]] SDL_Window* get_window() const;
	void set_window(SDL_Window* window);
	void should_exit();

private:
	SDL_Window* _window;
	bool _should_close;
};
