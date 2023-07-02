StatDeps - Static Dependency Graph
==================================

*A lightweight C++ dependency graph library for compile-time dependent resource management.*

This is an excerpt from a code base that **needs to use a dependency graph**:

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

See [`example/main.cpp`](example/main.cpp) to see how *StatDeps* addresses this issue!
