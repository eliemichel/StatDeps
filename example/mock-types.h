/*
 * Mock classes to have the example application look a bit like a typical
 * webgpu app.
 */

#pragma once

#include <string>
#include <vector>
#include <utility>
#include <iostream>

namespace glm {
struct uvec2 {
	uint32_t x;
	uint32_t y;
};
} // namespace glm

namespace wgpu {
struct Texture {
	glm::uvec2 size;
};
struct TextureView {};
} // namespace wgpu

namespace ImGui {
bool TextInput(const std::string& label, std::string* data) {
	return true;
}
} // namespace ImGui

std::pair<std::vector<uint8_t>, glm::uvec2> readImageFile(const std::string& path) {
	std::cout << "Read image file from '" << path << "'" << std::endl;
	uint32_t width = path == "another/file.png" ? 200 : 100;
	glm::uvec2 size = { width, width };
	std::vector<uint8_t> data(size.x * size.y);
	return { data, size };
}
wgpu::Texture createTexture(const glm::uvec2& size) {
	std::cout << "Create texture with size (" << size.x << ", " << size.y << ")" << std::endl;
	return { size };
}
void destroyTexture(wgpu::Texture texture) {
	std::cout << "Destroy texture with size (" << texture.size.x << ", " << texture.size.y << ")" << std::endl;
}
void uploadData(wgpu::Texture texture, const std::vector<uint8_t>& data) {
	std::cout << "Upload texture data" << std::endl;
}
wgpu::TextureView createTextureView(wgpu::Texture texture) {
	std::cout << "Create texture view with size (" << texture.size.x << ", " << texture.size.y << ")" << std::endl;
	return {};
}
void destroyTextureView(wgpu::TextureView textureView) {
	std::cout << "Destroy texture view" << std::endl;
}
