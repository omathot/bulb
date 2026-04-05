module;
#include <stb_image.h>
#include <SDL3/SDL.h>

module app;
import std;

TextureManager::TextureManager(SDL_GPUDevice* device) : _device(device) {}

Texture* TextureManager::load(const std::string& path) {
	// texture
	int tex_width = 0, tex_height = 0, tex_channels = 0;
	stbi_uc* pixels{};
	pixels = stbi_load(path.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
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
	auto texture = std::make_unique<Texture>(Texture(SDL_CreateGPUTexture(_device, &tex_info))); // NOLINT

	// big enough for both buffs + texture
	SDL_GPUTransferBufferCreateInfo transfer_info {
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = tex_size,
		.props = 0,
	};
	auto* transfer_buff = SDL_CreateGPUTransferBuffer(_device, &transfer_info);
	auto* data = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(_device, transfer_buff, false));
	if (!data) {
		SDL_Log("Failed to map TransferBuffer: %s", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(_device, transfer_buff);
		exit(1);
	}
	SDL_memcpy(data, pixels, tex_size);
	SDL_UnmapGPUTransferBuffer(_device, transfer_buff);
	stbi_image_free(pixels);

	auto* cmd_buff = SDL_AcquireGPUCommandBuffer(_device);
	auto* copy_pass = SDL_BeginGPUCopyPass(cmd_buff);

	// upload texture
	SDL_GPUTextureTransferInfo tex_src {
		.transfer_buffer = transfer_buff,
		.offset = 0,
	};
	SDL_GPUTextureRegion tex_dst {
		.texture = texture->_data,
		.w = static_cast<std::uint32_t>(tex_width),
		.h = static_cast<std::uint32_t>(tex_height),
		.d = 1,
	};
	SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);


	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(cmd_buff);
	SDL_ReleaseGPUTransferBuffer(_device, transfer_buff);
	_textures.push_back(std::move(texture));
	return _textures.back().get();
}

void TextureManager::cleanup() {
	for (auto& texture : _textures) {
		SDL_ReleaseGPUTexture(_device, texture->_data);
	}
	_textures.clear();
}

Texture* TextureManager::get_tex() const {
	return _textures.back().get();
}
