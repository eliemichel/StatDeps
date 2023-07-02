#pragma once

#include <vector>
#include <iostream>
#include <functional>

namespace statdeps {


/**
 * An empty struct representing the absence of context for a dependency node
 */
struct NoContext {};

/**
 * A Dependency Node represents a resource, described by a way to initialize
 * and terminate it. This resource init/terminate can be tied to a specific
 * context (typically an Application class).
 * 
 * Create is called to create the resource
 * Destroy is called to free the resource
 * Exists is called to check whether we need to call Create/Destroy
 * Ready State can be used as an alternative to Exists, to let the depsgraph
 * automatically manage the ready variable when creating and destroying the
 * resource.
 * 
 * A node is specialization of this template class. It is recommended to use
 * the DepsNodeBuilder class instead of explicitly filling all fields of
 * this template class.
 * 
 * Warnings:
 *  - There is no security against circular dependencies
 *  - There is no security against race conditions
 */
template <
	int N, // N is just an ID for pretty printing
	typename Context, // The type of the Context from which init and terminate are members
	void (Context::*createFn)(), // Create fonction, as a member of the context class
	void (Context::*destroyFn)(), // Destroy fonction, as a member of the context class
	bool (Context::*existsFn)() const, // Exists fonction, as a member of the context class
	bool Context::*readyState, // Ready state, as a member of the context class
	void (*noContextCreateFn)(), // Create function that is used when Context is set to "NoContext"
	void (*noContextDestroyFn)(), // Destroy function that is used when Context is set to "NoContext"
	bool (*noContextExistsFn)(), // Exists function that is used when Context is set to "NoContext"
	bool *noContextReadyState // Ready state that is used when Context is set to "NoContext"
>
struct DepsNode {
	typedef Context Context;
	static constexpr void PrettyPrint() { std::cout << "StaticDepsNode<" << N << ">" << std::endl; }

	static constexpr void Create(Context& ctx) { if (createFn) (ctx.*createFn)(); }

	static constexpr void Destroy(Context& ctx) { if (destroyFn) (ctx.*destroyFn)(); }

	static constexpr bool UseExists() { return existsFn; }

	static constexpr bool Exists(const Context& ctx) { static_assert(existsFn); return (ctx.*existsFn)(); }

	static constexpr bool UseReadyState() { return readyState; }

	static constexpr bool& ReadyState(Context& ctx) { static_assert(readyState); return ctx.*readyState; }

	// If the node has no context, allow any context to be passed, and use the
	// "no context" version of the create/destroy/exists functions.
	using HasNoContext = std::is_same<Context, NoContext>;

	template <typename AnyContext, typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr void Create(AnyContext&) { if (noContextCreateFn) noContextCreateFn(); }

	template <typename AnyContext, typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr void Destroy(AnyContext&) { if (noContextDestroyFn) noContextDestroyFn(); }

	template <typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr bool UseExists() { return noContextExistsFn; }

	template <typename AnyContext, typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr bool Exists(const AnyContext&) { static_assert(noContextExistsFn); return noContextExistsFn(); }

	template <typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr bool UseReadyState() { return noContextReadyState; }

	template <typename AnyContext, typename = typename std::enable_if_t<HasNoContext::value>>
	static constexpr bool& ReadyState(AnyContext&) { static_assert(noContextReadyState); return *noContextReadyState; }
};

/**
 * Since the DepsNode type has se many template parameters, it is more easily
 * specialized using this DepsNodeBuilder. This follows a typical Builder
 * design pattern, except on templated typenames. For instance:
 * 
 *   using MyNode = DepsNodeBuilder
 *       ::with_create<createMyResource>
 *       ::with_destroy<destroyMyResource>
 *       ::build;
 * 
 * is equivalent to:
 * 
 *   using MyNode = DepsNode<
 *       0, NoContext,
 *       nullptr, nullptr, nullptr, nullptr,
 *       createMyResource, destroyMyResource, nullptr, nullptr
 *       >;
 * 
 * The implementation is split into two state, depending on whether the
 * create/destroy/etc. function are members of a given Context class or if they
 * are simple functions.
 */
template <
	int N = 0,
	typename Context = NoContext,
	void (Context::* createFn)() = nullptr,
	void (Context::* destroyFn)() = nullptr,
	bool (Context::* existsFn)() const = nullptr,
	bool Context::*readyState = nullptr
>
struct DepsNodeBuilder_implWithContext {
	template <int NewN>
	using with_identifier = DepsNodeBuilder_implWithContext<NewN, Context, createFn, destroyFn, existsFn, readyState>;

	template <void (Context::*newCreateFn)()>
	using with_create = DepsNodeBuilder_implWithContext<N, Context, newCreateFn, destroyFn, existsFn, readyState>;

	template <void (Context::*newDestroyFn)()>
	using with_destroy = DepsNodeBuilder_implWithContext<N, Context, createFn, newDestroyFn, existsFn, readyState>;

	template <bool (Context::*newExistsFn)() const>
	using with_exists = DepsNodeBuilder_implWithContext<N, Context, createFn, destroyFn, newExistsFn, readyState>;

	template <bool Context::*newReadyState>
	using with_ready_state = DepsNodeBuilder_implWithContext<N, Context, createFn, destroyFn, existsFn, newReadyState>;

	using build = DepsNode<N, Context, createFn, destroyFn, existsFn, readyState, nullptr, nullptr, nullptr, nullptr>;
};
template <
	int N = 0,
	void (*createFn)() = nullptr,
	void (*destroyFn)() = nullptr,
	bool (*existsFn)() = nullptr,
	bool *readyState = nullptr
>
struct DepsNodeBuilder_implNoContext {
	template <int NewN>
	using with_identifier = DepsNodeBuilder_implNoContext<NewN, createFn, destroyFn, existsFn, readyState>;

	template <typename NewContext, typename = typename std::enable_if_t<!std::is_same_v<NewContext, NoContext>>>
	using with_context = DepsNodeBuilder_implWithContext<N, NewContext, nullptr, nullptr, nullptr, nullptr>;

	template <void (*newCreateFn)()>
	using with_create = DepsNodeBuilder_implNoContext<N, newCreateFn, destroyFn, existsFn, readyState>;

	template <void (*newDestroyFn)()>
	using with_destroy = DepsNodeBuilder_implNoContext<N, createFn, newDestroyFn, existsFn, readyState>;

	template <bool (*newExistsFn)()>
	using with_exists = DepsNodeBuilder_implNoContext<N, createFn, destroyFn, newExistsFn, readyState>;

	template <bool *newReadyState>
	using with_ready_state = DepsNodeBuilder_implNoContext<N, createFn, destroyFn, existsFn, newReadyState>;

	using build = DepsNode<N, NoContext, nullptr, nullptr, nullptr, nullptr, createFn, destroyFn, existsFn, readyState>;
};
using DepsNodeBuilder = DepsNodeBuilder_implNoContext<>;

/**
 * The type DepsEdge<A,B> means "A depends on B"
 */
template <typename A, typename B>
struct DepsEdge {
	using Dependee = A;
	using Dependency = B;
};

/**
 * A utility type to represent list of nodes/edges
 */
template <typename... Elements>
struct List {};

/**
 * The top-level type representing a dependency graph, including nodes and edges.
 * 
 * NB: Currently, the list of nodes is never used, so you may leave it empty.
 *     In practice node list is inferred from the edge list.
 * 
 * NB: Ns and Es must have the form List<...> (TODO: raise a static error if not)
 */
template <typename Ns, typename Es>
struct DepsGraph {
	using NodeList = Ns;
	using EdgeList = Es;
};


} // namespace statdeps
