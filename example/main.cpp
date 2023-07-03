#include "mock-types.h"

#include <statdeps/statdeps.hpp>

#include <string>
#include <vector>
#include <iostream>
#include <functional>

/**
 * An example of application loosly inspired on a skeleton of WebGPU app, which
 * is a case where there are a lot of inter-entity dependencies.
 */
class Application {
public:
	/**
	 * Make sure that everything we need is initialized, namely in this example
	 * the textureView supposedly used for rendering.
	 */
	void onInit();

	/**
	 * Listen for changes in the input data, triggering the recreation of some
	 * resources if necessary.
	 */
	void onGui();

private:
	std::string m_path = "some/file.jpg";
	glm::uvec2 m_size;
	std::vector<uint8_t> m_data;
	bool m_dataReady = false;
	wgpu::Texture m_texture;
	bool m_textureReady = false;
	wgpu::TextureView m_textureView;
	bool m_textureViewReady = false;
	bool m_fakeReady = false;

public:
	/**
	 * The builder pattern allows to easily create an alias with some default
	 * options, here we make sure all dependency nodes use the Application as
	 * context.
	 */
	using DepsNodeBuilder = statdeps::DepsNodeBuilder::with_context<Application>;

	/**
	 * In the most simple case, a resource is just an abstract node in the
	 * dependency graph.
	 */
	using PathResource = DepsNodeBuilder::build;

	void createData() {
		auto [data, size] = readImageFile(m_path);
		m_data = data;
		m_size = size;
	}
	void destroyData() {
		std::cout << "Clear data" << std::endl;
		m_data.clear();
	}

	/**
	 * More often, a dependency node is labelled with a mean to create and destroy
	 * the associated resource (defined just above).
	 *
	 * The dependency graph also needs a boolean where to store whether the
	 * resource has been created or not (alternatively, we can manage this
	 * ourselves in create/destroy and provide a "exists" callback that tells
	 * whether the resource is initialized).
	 */
	using DataResource = DepsNodeBuilder
		::with_create<&createData>
		::with_destroy<&destroyData>
		::with_ready_state<&Application::m_dataReady>
		::build;

	void createTextureA() {
		m_texture = createTexture(m_size);
		uploadData(m_texture, m_data);
	}
	void destroyTextureA() {
		destroyTexture(m_texture);
	}

	/**
	 * TODO: add a way not to reallocate the texture when the size did not change.
	 */
	using TextureResource = DepsNodeBuilder
		::with_create<&createTextureA>
		::with_destroy<&destroyTextureA>
		::with_ready_state<&Application::m_textureReady>
		::build;

	void createTextureViewA() {
		m_textureView = createTextureView(m_texture);
	}
	void destroyTextureViewA() {
		destroyTextureView(m_textureView);
	}
	using TextureViewResource = DepsNodeBuilder
		::with_create<&createTextureViewA>
		::with_destroy<&destroyTextureViewA>
		::with_ready_state<&Application::m_textureViewReady>
		::build;

	/**
	 * In order to check that the automatic dependency update does not create
	 * unused resources, we define here a dependency node that is never
	 * required later in the code, and should hence never gets created.
	 */
	void createFake() {
		throw std::runtime_error("This resource should never get created because we don't ask for it");
	}
	using FakeResource = DepsNodeBuilder
		::with_create<&createFake>
		::with_ready_state<&Application::m_fakeReady>
		::build;

	/**
	 * Finally we list the dependencies between nodes.
	 * TODO: Make the example a bit more interesting, with diamond graph etc.
	 */
	using DepsLinks = statdeps::List<
		// StaticDepsLink<A, B> means "A depends on B"
		statdeps::DepsEdge<DataResource, PathResource>,
		statdeps::DepsEdge<TextureResource, DataResource>,
		statdeps::DepsEdge<TextureViewResource, TextureResource>,
		statdeps::DepsEdge<FakeResource, TextureViewResource>
	>;
	using DepsGraph = statdeps::DepsGraph<statdeps::List<>, DepsLinks>;

	/**
	 * Utility aliases to pass the current object as the context when
	 * creating/destroying resources.
	 */
	template <typename DepsNode>
	void ensureExists() { statdeps::ensureExists(*this, DepsNode{}, DepsGraph{}); }
	template <typename DepsNode>
	void rebuild() { statdeps::rebuild(*this, DepsNode{}, DepsGraph{}); }
};

void Application::onInit() {
	std::cout << "* Init" << std::endl;
	// Initialize the texture view, and recursively all its dependencies before it.
	ensureExists<TextureViewResource>();

	std::cout << "* Init again" << std::endl;
	// Running a second time should not change anything
	ensureExists<TextureViewResource>();
}

void Application::onGui() {
	if (ImGui::TextInput("Path", &m_path)) {
		std::cout << "* Change texture path, same texture size" << std::endl;
		// Destroy and re-create the path resource, and as a consequence destroy and
		// re-create all its existing dependees.
		rebuild<PathResource>();
	}

	// Simulate a change of texture size
	std::cout << "* Change texture path, different size" << std::endl;
	m_path = "another/file.png";
	rebuild<PathResource>();
}

// A simple type to string conversion, to demo dependency walking
template <typename T>
std::string type_name(T) { return "<unknown>"; }
#define registerTypeName(T) template<> std::string type_name<T>(T) { return #T; }
registerTypeName(Application::PathResource)
registerTypeName(Application::DataResource)
registerTypeName(Application::TextureResource)
registerTypeName(Application::TextureViewResource)
registerTypeName(Application::FakeResource)

int main(int argc, char* argv[]) {
	using AllDependees = decltype(statdeps::allDependees(Application::TextureResource{}, Application::DepsGraph{}));
	std::cout << "All dependees of TextureResource:" << std::endl;
	statdeps::forEach(AllDependees{}, [](auto&& arg) { std::cout << " - " << type_name(arg) << std::endl; });
	std::cout << std::endl;

	using AllDependencies = decltype(statdeps::allDependencies(Application::TextureResource{}, Application::DepsGraph{}));
	std::cout << "All dependencies of TextureResource:" << std::endl;
	statdeps::forEach(AllDependencies{}, [](auto&& arg) { std::cout << " - " << type_name(arg) << std::endl; });
	std::cout << std::endl;

	using AllDependees2 = decltype(statdeps::allDependees(Application::PathResource{}, Application::DepsGraph{}));
	std::cout << "All dependees of PathResource:" << std::endl;
	statdeps::forEach(AllDependees2{}, [](auto&& arg) { std::cout << " - " << type_name(arg) << std::endl; });
	std::cout << std::endl;

	Application app;
	app.onInit();
	app.onGui();
	return 0;
}
