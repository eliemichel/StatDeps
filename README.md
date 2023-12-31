StatDeps - Static Dependency Graph
==================================

*A lightweight C++ dependency graph library for compile-time dependent resource management.*

## Example

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

With *StatDeps*, this ends up being something like:

```C++
void Application::onInit() {
	ensureExists<RenderLoopResource>();
}

void Application::onFrame() {
	if (m_shouldRebuildPipeline) {
		rebuild<BindGroupLayoutsResource>();
		m_shouldRebuildPipeline = false;
		m_shouldReallocateTextures = false;
	}

	if (m_shouldReallocateTextures) {
		rebuild<TexturesResource>();
		m_shouldReallocateTextures = false;
	}
}
```

**NB** This is a **static** library, meaning that after compilation this code is exactly the same as above (more or less).

Of course there is also some boilerplate for defining the dependencies between the different resources, it's not that magic. ;) See [`example/main.cpp`](example/main.cpp) to see how *StatDeps* addresses this issue!

## Overview

In a nutshell, the user defines some "Dependency Node" types like this one, which specify how to create/destroy/check existence of a resource:

```C++
using TextureResource = DepsNodeBuilder
	::with_create<&createTexture>
	::with_destroy<&destroyTexture>
	::with_ready_state<&Self::m_textureReady>
	::build;
```

Then one specifies the dependencies:

```C++
using DepsLinks = List<
	DepsEdge<TextureResource, DataResource>,
	DepsEdge<TextureViewResource, TextureResource>,
	// [...]
>;
```

And finally in the application one can simply call e.g., `ensureExists<TextureViewResource>();` to init everything needed to get the texture view, and `rebuild<SomeResource>()` to rebuild a resource and thus all the ones that depend on it.
