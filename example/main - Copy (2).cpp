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

struct NoContext {
	void noop() {}
};

/**
 * A Dependency Node represents a resource, described by a way to initialize
 * and terminate it. This resource init/terminate can be tied to a specific
 * context (typically an Application class).
 * 
 * Create is called to create the resource
 * Destroy is called to free the resource
 * Exists is called to check whether we need to call Create/Destroy
 * 
 * A node is specialization of this template class. It is recommended to use
 * the StaticDepsNodeBuilder class instead of explicitly filling all fields of
 * this template class.
 */
template <
	int N, // N is just an ID for pretty printing
	typename Context, // The type of the Context from which init and terminate are members
	void (Context::*createFn)(), // Create fonction, as a member of the context class
	void (Context::*destroyFn)(), // Destroy fonction, as a member of the context class
	bool (Context::*existsFn)() const, // Exists fonction, as a member of the context class
	void (*noContextCreateFn)(), // Create function that is used when Context is set to "NoContext"
	void (*noContextDestroyFn)(), // Destroy function that is used when Context is set to "NoContext"
	bool (*noContextExistsFn)() // Exists function that is used when Context is set to "NoContext"
>
struct StaticDepsNode{
	typedef Context Context;
	static constexpr void PrintName() { std::cout << "StaticDepsNode<" << N << ">" << std::endl; }

	static constexpr void Create(Context& ctx) { if (createFn) (ctx.*createFn)(); }

	static constexpr void Destroy(Context& ctx) { if (destroyFn) (ctx.*destroyFn)(); }

	static constexpr bool Exists(const Context& ctx, bool defaultValue) { return existsFn ? (ctx.*existsFn)() : defaultValue; }

	// If the node has no context, allow any context to be passed, and use the
	// "no context" version of the create/destroy/exists functions.
	using HasNoContext = std::is_same<Context, NoContext>;

	template <typename AnyContext, typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr void Create(AnyContext&) { if (noContextCreateFn) noContextCreateFn(); }

	template <typename AnyContext, typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr void Destroy(AnyContext&) { if (noContextDestroyFn) noContextDestroyFn(); }

	template <typename AnyContext, typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr bool Exists(const AnyContext&, bool defaultValue) { return noContextExistsFn ? noContextExistsFn() : defaultValue; }
};

template <
	int N = 0,
	typename Context = NoContext,
	void (Context::* createFn)() = nullptr,
	void (Context::* destroyFn)() = nullptr,
	bool (Context::* existsFn)() const = nullptr
>
struct StaticDepsNodeBuilder_implWithContext {
	template <int NewN>
	using with_identifier = StaticDepsNodeBuilder_implWithContext<NewN, Context, createFn, destroyFn, existsFn>;

	template <typename NewContext>
	using with_context = StaticDepsNodeBuilder_implWithContext<N, NewContext, nullptr, nullptr, nullptr>;

	template <void (Context::* newCreateFn)()>
	using with_create = StaticDepsNodeBuilder_implWithContext<N, Context, newCreateFn, destroyFn, existsFn>;

	template <void (Context::* newDestroyFn)()>
	using with_destroy = StaticDepsNodeBuilder_implWithContext<N, Context, createFn, newDestroyFn, existsFn>;

	template <bool (Context::* newExistsFn)() const>
	using with_exists = StaticDepsNodeBuilder_implWithContext<N, Context, createFn, destroyFn, newExistsFn>;

	using build = StaticDepsNode<N, Context, createFn, destroyFn, existsFn, nullptr, nullptr, nullptr>;
};
template <
	int N = 0,
	void (*createFn)() = nullptr,
	void (*destroyFn)() = nullptr,
	bool (*existsFn)() = nullptr
>
struct StaticDepsNodeBuilder_implNoContext {
	template <int NewN>
	using with_identifier = StaticDepsNodeBuilder_implNoContext<NewN, createFn, destroyFn, existsFn>;

	template <typename NewContext, typename = typename std::enable_if_t<!std::is_same_v<NewContext, NoContext>>>
	using with_context = StaticDepsNodeBuilder_implWithContext<N, NewContext, nullptr, nullptr, nullptr>;

	template <void (*newCreateFn)()>
	using with_create = StaticDepsNodeBuilder_implNoContext<N, newCreateFn, destroyFn, existsFn>;

	template <void (*newDestroyFn)()>
	using with_destroy = StaticDepsNodeBuilder_implNoContext<N, createFn, newDestroyFn, existsFn>;

	template <bool (*newExistsFn)()>
	using with_exists = StaticDepsNodeBuilder_implNoContext<N, createFn, destroyFn, newExistsFn>;

	using build = StaticDepsNode<N, NoContext, nullptr, nullptr, nullptr, createFn, destroyFn, existsFn>;
};
using StaticDepsNodeBuilder = StaticDepsNodeBuilder_implNoContext<>;

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

template <typename Context, typename Node, typename Graph>
constexpr void ensureExists(typename Context& ctx, Node, Graph) noexcept;

template <typename Context, typename Node, typename Graph>
constexpr void rebuild(typename Context& ctx, Node, Graph) noexcept;

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

template <typename Context, typename Node, typename Graph>
constexpr void ensureExists(typename Context& ctx, Node, Graph) noexcept {
	ensureExists(ctx, Node{}, Graph{}, Graph::EdgeList{});
	if (!Node::Exists(ctx, false)) {
		Node::Create(ctx);
	}
}

template <typename Context, typename Node, typename Graph, typename Dependency, typename... OtherEdges>
constexpr void ensureExists(typename Context& ctx, Node, Graph, List<StaticDepsLink<Node, Dependency>, OtherEdges...>) noexcept {
	ensureExists(ctx, Dependency{}, Graph{});
	ensureExists(ctx, Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Context, typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void ensureExists(typename Context& ctx, Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	ensureExists(ctx, Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Context, typename Node, typename Graph>
constexpr void ensureExists(typename Context& ctx, Node, Graph, List<>) noexcept {
}

//

template <typename Context, typename Node, typename Graph>
constexpr void rebuild(typename Context& ctx, Node, Graph) noexcept {
	destroyDependees(ctx, Node{}, Graph{}, Graph::EdgeList{});
	if (Node::Exists(ctx, true)) {
		Node::Destroy(ctx);
	}
	Node::Create(ctx);
	createDependees(ctx, Node{}, Graph{}, Graph::EdgeList{});
}

template <typename Context, typename Node, typename Graph, typename Dependee, typename... OtherEdges>
constexpr void destroyDependees(typename Context& ctx, Node, Graph, List<StaticDepsLink<Dependee, Node>, OtherEdges...>) noexcept {
	destroyDependees(ctx, Dependee{}, Graph{}, typename Graph::EdgeList{});
	if (Node::Exists(ctx, true)) {
		Dependee::Destroy(ctx);
		// TODO: mark as "was existing" to only rebuild what was already existing
		// (that's what the "Z" test below is about)
	}
	destroyDependees(ctx, Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Context, typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void destroyDependees(typename Context& ctx, Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	destroyDependees(ctx, Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Context, typename Node, typename Graph>
constexpr void destroyDependees(typename Context& ctx, Node, Graph, List<>) noexcept {
}

template <typename Context, typename Node, typename Graph, typename Dependee, typename... OtherEdges>
constexpr void createDependees(typename Context& ctx, Node, Graph, List<StaticDepsLink<Dependee, Node>, OtherEdges...>) noexcept {
	if (!Dependee::Exists(ctx, false)) {
		Dependee::Create(ctx);
	}
	createDependees(ctx, Dependee{}, Graph{}, typename Graph::EdgeList{});
	createDependees(ctx, Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Context, typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void createDependees(typename Context& ctx, Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	createDependees(ctx, Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Context, typename Node, typename Graph>
constexpr void createDependees(typename Context& ctx, Node, Graph, List<>) noexcept {
}

//

class Application2 {
public:
	void initA() {
		std::cout << "Init Resource A" << std::endl;
		m_aReady = true;
	}
	void terminateA() {
		std::cout << "Terminate Resource A" << std::endl;
		m_aReady = false;
	}
	bool readyA() const {
		return m_aReady;
	}

	void initB() {
		std::cout << "Init Resource B" << std::endl;
		m_bReady = true;
	}
	void terminateB() {
		std::cout << "Terminate Resource B" << std::endl;
		m_bReady = false;
	}
	bool readyB() const {
		return m_bReady;
	}

	void initC() {
		std::cout << "Init Resource C" << std::endl;
		m_cReady = true;
	}
	void terminateC() {
		std::cout << "Terminate Resource C" << std::endl;
		m_cReady = false;
	}
	bool readyC() const {
		return m_cReady;
	}

	void initZ() {
		std::cout << "ERROR Z" << std::endl;
		m_zReady = true;
		//throw std::runtime_error("Should not be initialized, not required!");
	}
	bool readyZ() const {
		return m_zReady;
	}

	void onInit();
	void onGui();


private:
	bool m_aReady = false;
	bool m_bReady = false;
	bool m_cReady = false;
	bool m_zReady = false;

private:
	using Self = Application2;
	using DepsNodeBuilder = StaticDepsNodeBuilder::with_context<Self>;

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
		::build;

	using DepsLinks = List<
		// StaticDepsLink<A, B> means "A depends on B"
		StaticDepsLink<dataDepsNode, pathDepsNode>,
		StaticDepsLink<textureDepsNode, dataDepsNode>,
		StaticDepsLink<textureViewDepsNode, textureDepsNode>
	>;
	using DepsGraph = StaticDepsGraph<List<>, DepsLinks>;

	// Utility aliases
	template <typename DepsNode>
	void ensureExists() { ::ensureExists(*this, DepsNode{}, DepsGraph{}); }
	template <typename DepsNode>
	void rebuild() { ::rebuild(*this, DepsNode{}, DepsGraph{}); }

private:
	std::string m_path;
	glm::uvec2 m_size;
	std::vector<uint8_t> m_data;
	wgpu::Texture m_texture;
	wgpu::TextureView m_textureView;
};

void Application2::onInit() {
	ensureExists<textureViewDepsNode>();
	ensureExists<textureViewDepsNode>();
}

void Application2::onGui() {
	rebuild<pathDepsNode>();
}

void mainStatic() {
	std::cout << "\n== Static deps test ==" << std::endl;

	Application2 app;
	app.onInit();
	app.onGui();
}

int main(int argc, char* argv[]) {
	mainDynamic();
	mainStatic();
	return 0;
}
