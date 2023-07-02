#pragma once

#include "depsgraph.hpp"

#include <vector>
#include <iostream>
#include <functional>

namespace statdeps {

// Declarations (public)

/**
 * Ensure that teh resource corresponding to the provided dependency node has
 * been created, which recursively means to ensure that all of its dependencies
 * have also been created.
 */
template <typename Context, typename Node, typename Graph>
constexpr void ensureExists(typename Context& ctx, Node, Graph) noexcept;

/**
 * Destroy and recreate the resource corresponding to a node, and to the same
 * for all of its dependees.
 */
template <typename Context, typename Node, typename Graph>
constexpr void rebuild(typename Context& ctx, Node, Graph) noexcept;

/**
 * Mostly for debug: list in the stdout the dependencies of a node.
 */
template <typename Node, typename Graph>
constexpr void printDependencies(Node, Graph) noexcept;

// Definitions (private)

// ensureExists()

template <typename Context, typename Node, typename Graph>
constexpr void ensureExists(typename Context& ctx, Node, Graph) noexcept {
	ensureDependenciesExist(ctx, Node{}, Graph{}, Graph::EdgeList{});

	if constexpr (Node::UseReadyState()) {
		bool& ready = Node::ReadyState(ctx);
		if (!ready) {
			Node::Create(ctx);
			ready = true;
		}
	}
	else if constexpr (Node::UseExists()) {
		if (!Node::Exists(ctx)) {
			Node::Create(ctx);
		}
	}
	else {
		Node::Create(ctx);
	}
}

template <typename Context, typename Node, typename Graph, typename Dependency, typename... OtherEdges>
constexpr void ensureDependenciesExist(typename Context& ctx, Node, Graph, List<DepsEdge<Node, Dependency>, OtherEdges...>) noexcept {
	ensureExists(ctx, Dependency{}, Graph{});
	ensureDependenciesExist(ctx, Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Context, typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void ensureDependenciesExist(typename Context& ctx, Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	ensureDependenciesExist(ctx, Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Context, typename Node, typename Graph>
constexpr void ensureDependenciesExist(typename Context& ctx, Node, Graph, List<>) noexcept {
}

// rebuild()

template <typename Context, typename Node, typename Graph>
constexpr void rebuild(typename Context& ctx, Node, Graph) noexcept {
	destroyDependees(ctx, Node{}, Graph{}, Graph::EdgeList{});

	if constexpr (Node::UseReadyState()) {
		bool& ready = Node::ReadyState(ctx);
		if (ready) {
			Node::Destroy(ctx);
			ready = false;
		}
	}
	else if constexpr(Node::UseExists()) {
		if (Node::Exists(ctx)) {
			Node::Destroy(ctx);
		}
	}
	else {
		Node::Destroy(ctx);
	}

	Node::Create(ctx);

	// TODO: Make sure to only recreate dependees that were existing before
	createDependees(ctx, Node{}, Graph{}, Graph::EdgeList{});
}

template <typename Context, typename Node, typename Graph, typename Dependee, typename... OtherEdges>
constexpr void destroyDependees(typename Context& ctx, Node, Graph, List<DepsEdge<Dependee, Node>, OtherEdges...>) noexcept {
	// Destroy dependees of dependees
	destroyDependees(ctx, Dependee{}, Graph{}, typename Graph::EdgeList{});

	if constexpr (Dependee::UseReadyState()) {
		bool& ready = Dependee::ReadyState(ctx);
		if (ready) {
			Dependee::Destroy(ctx);
			ready = false;
		}
	}
	else if constexpr (Node::UseExists()) {
		if (Dependee::Exists(ctx)) {
			Dependee::Destroy(ctx);
			// TODO: mark as "was existing" to only rebuild what was already existing
			// (that's what the "Z" test below is about)
		}
	}
	else {
		Dependee::Destroy(ctx);
	}

	// Destroy other dependees of the current node of interest
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
constexpr void createDependees(typename Context& ctx, Node, Graph, List<DepsEdge<Dependee, Node>, OtherEdges...>) noexcept {
	if constexpr (Dependee::UseReadyState()) {
		bool& ready = Dependee::ReadyState(ctx);
		if (!ready) {
			Dependee::Create(ctx);
			ready = true;
		}
	}
	else if constexpr (Node::UseExists()) {
		if (!Dependee::Exists(ctx)) {
			Dependee::Create(ctx);
		}
	}
	else {
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

// printDependencies()

template <typename Node, typename Graph>
constexpr void printDependencies(Node, Graph) noexcept {
	printDependencies(Node{}, Graph{}, Graph::EdgeList{});
}

template <typename Node, typename Graph, typename Dependency, typename... OtherEdges>
constexpr void printDependencies(Node, Graph, List<DepsEdge<Node, Dependency>, OtherEdges...>) noexcept {
	printDependencies(Dependency{}, Graph{});
	Dependency::PrettyPrint();
	printDependencies(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr void printDependencies(Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	printDependencies(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph>
constexpr void printDependencies(Node, Graph, List<>) noexcept {
}


} // namespace statdeps
