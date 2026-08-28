#pragma once

#include <cstddef>
#include <vector>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Non-owning view over one particle's row of neighbor indices.
		 *
		 * Exists so that the neighbor-consuming code (the SPH solvers and
		 * particle types in Phantom::Physics) does not have to care whether the
		 * row came from a CSRNeighborList (two flat arrays, one row = a slice of
		 * `indices`) or from a standalone std::vector<int> (what tests and
		 * ad-hoc call sites build by hand). Implicitly constructible from
		 * std::vector<int> for exactly that reason.
		 *
		 * The view does not own the storage: it is only valid while the
		 * container it was obtained from is alive and unmodified. Rows are
		 * always rebuilt wholesale (never appended to) in this codebase, so a
		 * view is only ever used within the pass that requested it.
		 */
		struct NeighborIndexView
		{
			const int* first = nullptr;
			const int* last = nullptr;

			NeighborIndexView() = default;

			NeighborIndexView(const int* first, const int* last) :
				first(first),
				last(last)
			{}

			// Implicit on purpose -- see the class doc.
			NeighborIndexView(const std::vector<int>& indices) :
				first(indices.data()),
				last(indices.data() + indices.size())
			{}

			const int* begin() const { return first; }
			const int* end() const { return last; }
			size_t size() const { return static_cast<size_t>(last - first); }
			bool empty() const { return first == last; }
		};

	}
}
