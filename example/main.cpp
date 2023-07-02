#include <statdeps/statdeps.hpp>

#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <functional>
#include <variant>

// Mock classes

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

std::pair<std::vector<uint8_t>,glm::uvec2> readImageFile(const std::string& path) {
	std::cout << "Read image file from '" << path << "'" << std::endl;
	glm::uvec2 size = { 100, 100 };
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

class Application2 {
public:
	void onInit();
	void onGui();

private:
	std::string m_path;
	glm::uvec2 m_size;
	std::vector<uint8_t> m_data;
	bool m_dataReady = false;
	wgpu::Texture m_texture;
	bool m_textureReady = false;
	wgpu::TextureView m_textureView;
	bool m_textureViewReady = false;

private:
	using Self = Application2;
	using DepsNodeBuilder = statdeps::DepsNodeBuilder::with_context<Self>;

	using pathDepsNode = DepsNodeBuilder::build;

	void createData() {
		auto [data, size] = readImageFile(m_path);
		m_data = data;
		m_size = size;
	}
	void destroyData() {
		m_data.clear();
	}
	using dataDepsNode = DepsNodeBuilder
		::with_create<&createData>
		::with_destroy<&destroyData>
		::with_ready_state<&Self::m_dataReady>
		::build;

	void createTextureA() {
		m_texture = createTexture(m_size);
		uploadData(m_texture, m_data);
	}
	void destroyTextureA() {
		destroyTexture(m_texture);
	}
	using textureDepsNode = DepsNodeBuilder
		::with_create<&createTextureA>
		::with_destroy<&destroyTextureA>
		::with_ready_state<&Self::m_textureReady>
		::build;

	void createTextureViewA() {
		m_textureView = createTextureView(m_texture);
	}
	void destroyTextureViewA() {
		destroyTextureView(m_textureView);
	}
	using textureViewDepsNode = DepsNodeBuilder
		::with_create<&createTextureViewA>
		::with_destroy<&destroyTextureViewA>
		::with_ready_state<&Self::m_textureViewReady>
		::build;

	using DepsLinks = statdeps::List<
		// StaticDepsLink<A, B> means "A depends on B"
		statdeps::DepsEdge<dataDepsNode, pathDepsNode>,
		statdeps::DepsEdge<textureDepsNode, dataDepsNode>,
		statdeps::DepsEdge<textureViewDepsNode, textureDepsNode>
	>;
	using DepsGraph = statdeps::DepsGraph<statdeps::List<>, DepsLinks>;

	// Utility aliases
	template <typename DepsNode>
	void ensureExists() { statdeps::ensureExists(*this, DepsNode{}, DepsGraph{}); }
	template <typename DepsNode>
	void rebuild() { statdeps::rebuild(*this, DepsNode{}, DepsGraph{}); }
};

void Application2::onInit() {
	ensureExists<textureViewDepsNode>();
	ensureExists<textureViewDepsNode>();
}

void Application2::onGui() {
	rebuild<pathDepsNode>();
}

int main(int argc, char* argv[]) {
	Application2 app;
	app.onInit();
	app.onGui();
	return 0;
}
