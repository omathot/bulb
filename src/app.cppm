module;
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


export module app;
import std;

const std::string FRAGMENT_SHADER_PATH = "/home/omathot/dev/cpp/bulb/shaders/fragment.spv";
const std::string VERTEX_SHADER_PATH = "/home/omathot/dev/cpp/bulb/shaders/vertex.spv";

const std::string VIKING_MODEL = "/home/omathot/dev/cpp/bulb/assets/models/viking_room.obj";
const std::string VIKING_TEXTURE = "/home/omathot/dev/cpp/bulb/assets/textures/viking_room.png";

const std::string TITANIC_MODEL = "/home/omathot/dev/cpp/bulb/assets/models/Titanic.obj";
const std::string TITANIC_TEXTURE = "/home/omathot/dev/cpp/bulb/assets/textures/Titanic.jpeg";

const std::string MINECRAFT_MODEL = "/home/omathot/dev/cpp/bulb/assets/models/minecraft_castle.obj";
const std::string MINECRAFT_TEXTURE = "/home/omathot/dev/cpp/bulb/assets/textures/minecraft_castle.jpg";

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
	glm::vec3 normal;

	bool operator==(const Vertex &other) const {
		return pos == other.pos &&
			color == other.color &&
			uv == other.uv &&
			normal == other.normal;
	}
};
// 0x9e3779b9 magic constant is from golden ratio? apparently spreads bits well and avoids collision issues.
inline void hash_combine(size_t& seed, size_t hash) {
	seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
template <>
struct std::hash<Vertex> {
	size_t operator()(Vertex const &vertex) const noexcept {
		size_t h = 0;
		hash_combine(h, hash<glm::vec3>()(vertex.pos));
		hash_combine(h, hash<glm::vec3>()(vertex.color));
		hash_combine(h, hash<glm::vec2>()(vertex.uv));
		hash_combine(h, hash<glm::vec3>()(vertex.normal));
		return h;
	}
};

struct UniformBuffer {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};
struct LightBuffer {
	alignas(16) glm::vec3 light_dir;
	alignas(16) glm::vec3 light_color;
	alignas(16) glm::vec3 camera_pos;
	float enabled = 1.0f;
};

struct Controller {
	glm::vec3 _pos{{}};
	float _angle = 0;
	std::uint32_t _speed = 5;
	std::uint32_t _rot_speed = 50;
	bool move_left = false;
	bool move_right = false;
	bool move_further = false;
	bool move_closer = false;
	bool move_up = false;
	bool move_down = false;
	bool pause = false;
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

struct Camera {
	glm::vec3 _pos{2.0f, 2.0f, 2.0f};
	float _yaw = -135.0f;
	float _pitch = -30.0f;
	float _speed = 5.0f;
	float _sensitivity = 0.1f;

	bool move_left = false;
	bool move_right = false;
	bool move_forwards = false;
	bool move_backwards = false;
	bool move_up = false;
	bool move_down = false;

	[[nodiscard]] glm::vec3 front() const {
		return glm::normalize(glm::vec3{
			cos(glm::radians(_yaw)) * cos(glm::radians(_pitch)),
			sin(glm::radians(_pitch)),
			sin(glm::radians(_yaw)) * cos(glm::radians(_pitch))
		});
	}
	[[nodiscard]] glm::vec3 right() const {
		return glm::normalize(glm::cross(front(), glm::vec3(0.0f, 1.0f, 0.0f)));
	}
	[[nodiscard]] glm::mat4 view_matrix() const {
		return glm::lookAt(_pos, _pos + front(), glm::vec3(0.0f, 1.0f, 0.0f));
	}
};

enum class ModelRequest : std::uint8_t {
	minecraft,
	titanic,
	viking,
	unknown,
};
enum class ScaleRequest : std::uint8_t {
	scaled_up,
	scaled_down,
	unknown,
};

export class App {
public:
	App();

	void arguments(int argc, char **argv);
	void init();
	[[nodiscard]] SDL_AppResult iterate();
	[[nodiscard]] SDL_AppResult handle_event(SDL_Event* event);
	void cleanup();

private:
	SDL_Window* _window = nullptr;
	SDL_GPUDevice* _device = nullptr;

	std::unique_ptr<TextureManager> _texture_manager;
	SDL_GPUTexture* _depth_texture = nullptr;

	std::vector<Vertex> _vertices;
	std::vector<std::uint32_t> _indices;
	SDL_GPUBuffer* _vertex_buff = nullptr;
	SDL_GPUBuffer* _index_buff = nullptr;

	SDL_GPUGraphicsPipeline* _graphics_pipeline = nullptr;
	SDL_GPUGraphicsPipeline* _wireframe_pipeline = nullptr;
	SDL_GPUSampler* _sampler = nullptr;

	std::unique_ptr<Camera> _camera;

	float _dt{};
	bool _should_exit = false;
	bool _mouse_captured = false;
	bool _lighting_enabled = false;
	bool _wireframe = false;
	ModelRequest _model_request = ModelRequest::unknown;
	ScaleRequest _scale_request = ScaleRequest::unknown;

	void set_window(SDL_Window* window);
	void set_device(SDL_GPUDevice* device);
	void setup_gpu_resources();
	void load_model();
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
