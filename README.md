Depsgraph
=========

*A lightweight dependency graph library for dependent resource management.*

This is an excerpt from a code base that **needs to use a depsgraph**:

```C++
void Application::onInit() {
	// The order matters here, it roughly corresponds to dependencies
	initWindow();
	initDevice();
	initSwapChain();
	initGui();
	initBindGroupLayouts();
	initComputePipelines();
	initTextures();
	initTextureViews();
	initBuffers();
	initSampler();
	initBindGroups();
	initRenderLoop();
}

void Application::onFrame() {
	if (m_shouldRebuildPipeline) {
		// We must terminate in the right order everything that depends on the pipeline
		terminateRenderLoop();
		terminateBindGroups();
		terminateBuffers();
		terminateTextureViews();
		terminateTextures();

		// Then rebuild the pipeline
		terminateComputePipelines();
		terminateBindGroupLayouts();
		initBindGroupLayouts();
		initComputePipelines();

		// Then re-init all the dependencies
		initTextures();
		initTextureViews();
		initBuffers();
		initBindGroups();
		initRenderLoop();
		m_shouldRebuildPipeline = false;
		m_shouldReallocateTextures = false;
	}

	if (m_shouldReallocateTextures) {
		// Similar behavior
		terminateRenderLoop();
		terminateBindGroups();
		terminateBuffers();
		terminateTextureViews();
		terminateTextures();
		initTextures();
		initTextureViews();
		initBuffers();
		initBindGroups();
		initRenderLoop();
		m_shouldReallocateTextures = false;
	}

	// [...]
}
```

With a dependency graph, this looks like:

```C++
void Application::onInit() {
	// Make sure the render loop is initialized, as well as all its
	// dependencies, in the right order.
	m_renderLoopDepsNode.ensureInit();
}

void Application::onFrame() {
	if (m_shouldRebuildPipeline) {
		// Rebuild the resource, as well as all the resources that depends on it
		m_bindGroupLayoutsDepsNode.rebuild();
		m_shouldRebuildPipeline = false;
		m_shouldReallocateTextures = false;
	}

	if (m_shouldReallocateTextures) {
		// Similar behavior
		m_texturesDepsNode.rebuild();
		m_shouldReallocateTextures = false;
	}

	// [...]
}

void Application::setupDependencies() {
	m_windowDepsNode = DepsNode(initWindow, terminateWindow);
	m_deviceDepsNode = DepsNode(initDeviceDepsNode, terminateDevice);
	m_swapChainDepsNode = DepsNode(initSwapChainDepsNode, terminateSwapChain);
	m_guiDepsNode = DepsNode(initGuiDepsNode, terminateGui);
	m_bindGroupLayoutsDepsNode = DepsNode(initBindGroupLayoutsDepsNode, terminateBindGroupLayouts);
	m_computePipelinesDepsNode = DepsNode(initComputePipelinesDepsNode, terminateComputePipelines);
	m_texturesDepsNode = DepsNode(initTexturesDepsNode, terminateTextures);
	m_textureViewsDepsNode = DepsNode(initTextureViewsDepsNode, terminateTextureViews);
	m_buffersDepsNode = DepsNode(initBuffersDepsNode, terminateBuffers);
	m_samplerDepsNode = DepsNode(initSamplerDepsNode, terminateSampler);
	m_bindGroupsDepsNode = DepsNode(initBindGroupsDepsNode, terminateBindGroups);
	m_renderLoopDepsNode = DepsNode(initRenderLoopDepsNode, terminateRenderLoop);
}
```


Let's take an example:

 1. I have a text input which contains a file path.
 2. **Depending on** this path, I load the file's content data.
 3. **Depending on** this content, I upload the file's data to a GPU texture.

If the texture size changes, I need to destroy and rebuild the texture, otherwise I re-upload on the same one.


```C++
class Application {
	std::string m_path;
	glm::uvec2 m_size;
	std::vector<uint8_t> m_data;
	wgpu::Texture m_texture;
	wgpu::TextureView m_textureView;

	DepsNode m_pathDepsNode;
	DepsNode m_dataDepsNode;
	DepsNode m_textureDepsNode;
	DepsNode m_textureViewDepsNode;
}

void Application::setupDependencies() {
	m_dataDepsNod
	.init([&]() {
		auto [data, size] = readImageFile(m_path);
		m_data = data;
		m_size = size;
	})
	.terminate([&](){
		m_data.clear();
	})
	.dependsOn(m_pathDepsNode);

	m_textureDepsNode
	.init([&]() {
		m_texture = createTexture(m_size);
		uploadData(m_texture, m_data);
	})
	.terminate([&](){
		destroyTexture(m_texture);
	})
	.dependsOn(m_dataDepsNod);

	m_textureViewDepsNode
	.init([&]() {
		m_textureView = createTextureView(m_texture);
	})
	.terminate([&](){
		destroyTextureView(m_textureView);
	})
	.dependsOn(m_textureDepsNode);
}

void Application::onGui() {
	if (ImGui::TextInput("Path", &m_path)) {
		// Value changed:
		m_pathDepsNode.rebuild();
	}
}
```
