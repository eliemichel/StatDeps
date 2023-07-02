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

// Core lib

namespace depsgraph {

/**
 * Warnings:
 *  - There is no security against circular dependencies
 *  - There is no security against race conditions
 *  - DepsNodes are identified by their address, do not move them in memory
 */
class DepsNode {
public:
	DepsNode() {}
	DepsNode(const DepsNode& other) = delete;
	DepsNode& operator=(const DepsNode& other) = delete;

public:
	// Builder functions
	template <typename Lambda>
	DepsNode& init(Lambda callback) {
		m_initCallback = callback;
		return *this;
	}

	template <typename Lambda>
	DepsNode& terminate(Lambda callback) {
		m_terminateCallback = callback;
		return *this;
	}

	DepsNode& dependsOn(DepsNode& other) {
		m_dependencies.push_back(&other);
		other.m_dependees.push_back(this);
		return *this;
	}

	// Runtime functions
	void ensureInit() {
		if (m_ready) return;
		for (auto d : m_dependencies) {
			d->ensureInit();
		}
		if (m_initCallback) m_initCallback();
		m_ready = true;
	}

	void rebuild() {
		doTerminate();
		doInitHot();
	}

private:
	void doInitHot() {
		if (m_initCallback) m_initCallback();
		m_ready = true;
		for (auto d : m_dependees) {
			if (d->m_hot) {
				d->doInitHot();
			}
		}
	}

	void doTerminate() {
		for (auto d : m_dependees) {
			d->m_hot = d->m_ready;
			d->doTerminate();
		}
		if (m_ready && m_terminateCallback) m_terminateCallback();
		m_ready = false;
	}

private:
	bool m_ready = false;
	bool m_hot = false; // used to only reinit what was init before rebuild()
	std::function<void()> m_initCallback;
	std::function<void()> m_terminateCallback;
	std::vector<DepsNode*> m_dependencies;
	std::vector<DepsNode*> m_dependees;
};
} // namespace depsgraph

// Example

using namespace depsgraph;

class Application {
public:
	Application() { std::cout << "Application ctor" << std::endl; }
	~Application() { std::cout << "Application dtor" << std::endl; }
	Application(const Application& other) = delete;
	Application& operator=(const Application& other) = delete;

	void setupDependencies();
	void onInit();
	void onGui();

private:
	std::string m_path;
	glm::uvec2 m_size;
	std::vector<uint8_t> m_data;
	wgpu::Texture m_texture;
	wgpu::TextureView m_textureView;

	DepsNode m_pathDepsNode;
	DepsNode m_dataDepsNode;
	DepsNode m_textureDepsNode;
	DepsNode m_textureViewDepsNode;
	DepsNode m_unusedDepsNode;
};

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

void Application::setupDependencies() {
	m_unusedDepsNode
	.init([&]() {
		throw std::runtime_error("ERROR: This should not be initialized because it is not required!");
	})
	.dependsOn(m_textureDepsNode);

	m_dataDepsNode
	.init([this]() {
		auto [data, size] = readImageFile(m_path);
		m_data = data;
		m_size = size;
	})
	.terminate([this](){
		m_data.clear();
	})
	.dependsOn(m_pathDepsNode);

	m_textureDepsNode
	.init([this]() {
		m_texture = createTexture(m_size);
		uploadData(m_texture, m_data);
	})
	.terminate([this](){
		destroyTexture(m_texture);
	})
	.dependsOn(m_dataDepsNode);

	m_textureViewDepsNode
	.init([this]() {
		m_textureView = createTextureView(m_texture);
	})
	.terminate([this](){
		destroyTextureView(m_textureView);
	})
	.dependsOn(m_textureDepsNode);
}

void Application::onInit() {
	m_textureViewDepsNode.ensureInit();
	m_textureViewDepsNode.ensureInit();
}

void Application::onGui() {
	if (ImGui::TextInput("Path", &m_path)) {
		std::cout << "Change file path to '" << m_path << "'" << std::endl;
		// Value changed:
		m_pathDepsNode.rebuild();
	}
}

void mainDynamic() {
	std::cout << "== Dynamic deps test ==" << std::endl;
	Application app;
	app.setupDependencies();
	app.onInit();
	app.onGui();
}

// Attempt at making a static version of the core lib

typedef void(*InitFn)();
typedef void(*TerminateFn)();
void noop() {}

template <int N, InitFn init = noop, TerminateFn terminate = noop>
struct StaticDepsNode{
	static constexpr void PrintName() { std::cout << "StaticDepsNode<" << N << ">" << std::endl; }
	static constexpr void Init() { init(); }
	static constexpr void Terminate() { terminate(); }
};

// "A depends on B"
template <typename A, typename B>
struct StaticDepsLink {
	using Dependee = A;
	using Dependency = B;
};

template <typename Ns, typename Es>
struct StaticDepsGraph {
	using NodeList = Ns;
	using EdgeList = Es;
};

template <typename... Elements>
struct List {};

// Declaration

template <typename Node, typename Graph>
constexpr void printDependencies(Node, Graph) noexcept;

template <typename Node, typename Graph>
constexpr void ensureInit(Node, Graph) noexcept;

template <typename Node, typename Graph>
constexpr void rebuild(Node, Graph) noexcept;

// Definitions

template <typename Node, typename Graph>
constexpr void printDependencies(Node, Graph) noexcept {
	printDependencies(Node{}, Graph{}, Graph::EdgeList{});
}

template <typename Node, typename Graph, typename Dependency, typename... OtherEdges>
constexpr void printDependencies(Node, Graph, List<StaticDepsLink<Node, Dependency>, OtherEdges...>) noexcept {
	printDependencies(Dependency{}, Graph{});
	Dependency::PrintName();
	printDependencies(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void printDependencies(Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	printDependencies(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph>
constexpr void printDependencies(Node, Graph, List<>) noexcept {
}

//

template <typename Node, typename Graph>
constexpr void ensureInit(Node, Graph) noexcept {
	ensureInit(Node{}, Graph{}, Graph::EdgeList{});
	Node::Init();
}

template <typename Node, typename Graph, typename Dependency, typename... OtherEdges>
constexpr void ensureInit(Node, Graph, List<StaticDepsLink<Node, Dependency>, OtherEdges...>) noexcept {
	ensureInit(Dependency{}, Graph{});
	ensureInit(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void ensureInit(Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	ensureInit(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph>
constexpr void ensureInit(Node, Graph, List<>) noexcept {
}

//

template <typename Node, typename Graph>
constexpr void rebuild(Node, Graph) noexcept {
	terminateDependees(Node{}, Graph{}, Graph::EdgeList{});
	Node::Terminate();
	Node::Init();
	initDependees(Node{}, Graph{}, Graph::EdgeList{});
}

template <typename Node, typename Graph, typename Dependee, typename... OtherEdges>
constexpr void terminateDependees(Node, Graph, List<StaticDepsLink<Dependee, Node>, OtherEdges...>) noexcept {
	terminateDependees(Dependee{}, Graph{}, typename Graph::EdgeList{});
	Dependee::Terminate();
	terminateDependees(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void terminateDependees(Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	terminateDependees(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph>
constexpr void terminateDependees(Node, Graph, List<>) noexcept {
}

template <typename Node, typename Graph, typename Dependee, typename... OtherEdges>
constexpr void initDependees(Node, Graph, List<StaticDepsLink<Dependee, Node>, OtherEdges...>) noexcept {
	Dependee::Init();
	initDependees(Dependee{}, Graph{}, typename Graph::EdgeList{});
	initDependees(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void initDependees(Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	initDependees(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph>
constexpr void initDependees(Node, Graph, List<>) noexcept {
}

//

void initA() {
	std::cout << "Init Resource A" << std::endl;
}
void terminateA() {
	std::cout << "Terminate Resource A" << std::endl;
}
void initB() {
	std::cout << "Init Resource B" << std::endl;
}
void terminateB() {
	std::cout << "Terminate Resource B" << std::endl;
}
void initC() {
	std::cout << "Init Resource C" << std::endl;
}
void terminateC() {
	std::cout << "Terminate Resource C" << std::endl;
}
void initZ() {
	std::cout << "ERROR Z" << std::endl;
	//throw std::runtime_error("Should not be initialized, not required!");
}

class Application {
public:
	void initA() {
		std::cout << "Init Resource A in application" << std::endl;
	}
	void terminateA() {
		std::cout << "Terminate Resource A in application" << std::endl;
	}
};

using depsNodeA = StaticDepsNode<1, initA, terminateA>;
using depsNodeB = StaticDepsNode<2, initB, terminateB>;
using depsNodeC = StaticDepsNode<3, initC, terminateC>;
using depsNodeZ = StaticDepsNode<4, initZ>;
using nodes = List<depsNodeA, depsNodeA, depsNodeC>;

using depsLinkAB = StaticDepsLink<depsNodeA, depsNodeB>;
using depsLinkBC = StaticDepsLink<depsNodeB, depsNodeC>;
using depsLinkZA = StaticDepsLink<depsNodeZ, depsNodeA>;
using edges = List<depsLinkAB, depsLinkBC, depsLinkZA>;

using graph = StaticDepsGraph<nodes, edges>;

void mainStatic() {
	std::cout << "\n== Static deps test ==" << std::endl;
	std::cout << "Dependencies of "; depsNodeA::PrintName();
	printDependencies(depsNodeA{}, graph{});

	std::cout << "Ensure init of "; depsNodeA::PrintName();
	ensureInit(depsNodeA{}, graph{});

	std::cout << "Rebuilding "; depsNodeB::PrintName();
	rebuild(depsNodeB{}, graph{});
}

int main(int argc, char* argv[]) {
	mainDynamic();
	mainStatic();
	return 0;
}
