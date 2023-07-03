#pragma once

#include "depsgraph.hpp"

#include <vector>
#include <iostream>
#include <functional>

namespace statdeps {

////////////////////////////////////////////////////
#pragma region [Declarations, list operations (public)]

/**
 * Run a callback for each element of a list
 */
template <typename... Items, typename Lambda>
constexpr void forEach(List<Items...>, Lambda callback) noexcept;

/**
 * Basic operations on lists
 */
template <typename NewItem, typename... Items>
constexpr auto prepend(NewItem, List<Items...>) noexcept;

template <typename NewItem, typename... Items>
constexpr auto append(List<Items...>, NewItem) noexcept;

template <typename... Items, typename... OtherItems>
constexpr auto concat(List<Items...>, List<OtherItems...>) noexcept;

template <typename... Items>
constexpr auto revert(List<Items...>) noexcept;

#pragma endregion

////////////////////////////////////////////////////
#pragma region [Declarations, depsnode operations (public)]

/**
 * Higher-level function around the DepsNode API, which picks either Exists()
 * or ReadyState() depending on which one is available.
 */

/**
 * Returns whether the resource corresponding to a dependency node has been
 * created (and not destroyed since then). This uses ReadyState if available,
 * otherwise it uses Exists(). And if none of them is available, it returns the
 * default value.
 */
template <typename Context, typename Node>
constexpr bool doesResourceExist(Context& ctx, Node, bool defaultValue);

/**
 * Create the resource corresponding to a dependency node, if it does not
 * already exist, and update the ready state if appropriate.
 */
template <typename Context, typename Node>
constexpr void createResource(Context& ctx, Node);

/**
 * Destroy the resource corresponding to a dependency node if exists, and
 * update the ready state if appropriate.
 */
template <typename Context, typename Node>
constexpr void destroyResource(Context& ctx, Node);

#pragma endregion

////////////////////////////////////////////////////
#pragma region [Declarations, depsgraph operations (public)]

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
 * Get all nodes on which the given node depends, be it directly or indirectly.
 * Returned nodes are sorted by dependency order (the first one depends on nothing).
 */
template <typename Node, typename Graph>
constexpr auto allDependencies(Node, Graph) noexcept;

/**
 * Get all nodes that depend directly or indirectly on a given one.
 * Returned nodes are sorted by dependency order (the first one depends
 * directly on the given node).
 */
template <typename Node, typename Graph>
constexpr auto allDependees(Node, Graph) noexcept;

/**
 * Mostly for debug: list in the stdout the dependencies of a node.
 */
template <typename Node, typename Graph>
constexpr void printDependencies(Node, Graph) noexcept;

#pragma endregion

////////////////////////////////////////////////////
#pragma region [Definitions, list operations (private)]

// forEachInList()

template <typename FirstItem, typename... OtherItems, typename Lambda>
constexpr void forEach(List<FirstItem, OtherItems...>, Lambda lambda) noexcept {
	lambda(FirstItem{});
	forEach(List<OtherItems...>{}, lambda);
}

template <typename Lambda>
constexpr void forEach(List<>, Lambda) noexcept {
}

// prepend()

template <typename NewItem, typename... Items>
constexpr auto prepend(NewItem, List<Items...>) noexcept {
	return List<NewItem, Items...>{};
}

// append()

template <typename NewItem, typename... Items>
constexpr auto append(List<Items...>, NewItem) noexcept {
	return List<Items..., NewItem>{};
}

// concat()

template <typename... Items, typename... OtherItems>
constexpr auto concat(List<Items...>, List<OtherItems...>) noexcept {
	return List<Items..., OtherItems...>{};
}

// revert()

template <typename FirstItem, typename... Items>
constexpr auto revert(List<FirstItem, Items...>) noexcept {
	return append(revert(List<Items...>{}), FirstItem{});
}

template <>
constexpr auto revert(List<>) noexcept {
	return List<>{};
}

#pragma endregion

////////////////////////////////////////////////////
#pragma region [Definitions, node operations (private)]

template <typename Context, typename Node>
constexpr bool doesResourceExist(Context& ctx, Node, bool defaultValue) {
	if constexpr (Node::UseReadyState()) {
		return Node::ReadyState(ctx);
	}
	else if constexpr (Node::UseExists()) {
		return Node::Exists(ctx);
	}
	else {
		return defaultValue;
	}
}

template <typename Context, typename Node>
constexpr void createResource(Context& ctx, Node) {
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

template <typename Context, typename Node>
constexpr void destroyResource(Context& ctx, Node) {
	if constexpr (Node::UseReadyState()) {
		bool& ready = Node::ReadyState(ctx);
		if (ready) {
			Node::Destroy(ctx);
			ready = false;
		}
	}
	else if constexpr (Node::UseExists()) {
		if (Node::Exists(ctx)) {
			Node::Destroy(ctx);
		}
	}
	else {
		Node::Destroy(ctx);
	}
}
#pragma endregion

////////////////////////////////////////////////////
#pragma region [Definitions, depsgraph operations (private)]

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
	rebuild(ctx, Node{}, Graph{}, revert(allDependees(Node{}, Graph{})));
}

template <typename Context, typename Node, typename Graph, typename FirstDependee, typename... OtherDependees>
constexpr void rebuild(typename Context& ctx, Node, Graph, List<FirstDependee, OtherDependees...>) noexcept {
	bool shouldRecreate = doesResourceExist(ctx, FirstDependee{}, true);
	destroyResource(ctx, FirstDependee{});
	
	rebuild(ctx, Node{}, Graph{}, List<OtherDependees...>{});

	if (shouldRecreate) {
		createResource(ctx, FirstDependee{});
	}
}

template <typename Context, typename Node, typename Graph>
constexpr void rebuild(typename Context& ctx, Node, Graph, List<>) noexcept {
	destroyResource(ctx, Node{});
	createResource(ctx, Node{});
}

// allDependencies()

template <typename Node, typename Graph>
constexpr auto allDependencies(Node, Graph) noexcept {
	return allDependencies(Node{}, Graph{}, Graph::EdgeList{});
}

template <typename Node, typename Graph, typename Dependency, typename... OtherEdges>
constexpr auto allDependencies(Node, Graph, List<DepsEdge<Node, Dependency>, OtherEdges...>) noexcept {
	auto otherDependencies = allDependencies(Node{}, Graph{}, List<OtherEdges...>{});
	auto dependenciesOfDependencies = allDependencies(Dependency{}, Graph{});
	return concat(dependenciesOfDependencies, prepend(Dependency{}, otherDependencies)); // TODO
}

template <typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr auto allDependencies(Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	return allDependencies(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph>
constexpr auto allDependencies(Node, Graph, List<>) noexcept {
	return List<>{};
}

// allDependees()

template <typename Node, typename Graph>
constexpr auto allDependees(Node, Graph) noexcept {
	return allDependees(Node{}, Graph{}, Graph::EdgeList{});
}

template <typename Node, typename Graph, typename Dependee, typename... OtherEdges>
constexpr auto allDependees(Node, Graph, List<DepsEdge<Dependee, Node>, OtherEdges...>) noexcept {
	auto otherDependees = allDependees(Node{}, Graph{}, List<OtherEdges...>{});
	auto dependeesOfDependees = allDependees(Dependee{}, Graph{});
	return prepend(Dependee{}, concat(dependeesOfDependees, otherDependees)); // TODO
}

template <typename Node, typename Graph, typename FirstEdge, typename... OtherEdges>
constexpr auto allDependees(Node, Graph, List<FirstEdge, OtherEdges...>) noexcept {
	return allDependees(Node{}, Graph{}, List<OtherEdges...>{});
}

template <typename Node, typename Graph>
constexpr auto allDependees(Node, Graph, List<>) noexcept {
	return List<>{};
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

#pragma endregion

} // namespace statdeps
