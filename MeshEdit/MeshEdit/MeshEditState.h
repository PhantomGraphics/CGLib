#pragma once
// Mesh edit session state and the argument parsing around it (docs/todo/PLAN_phantomstudio_mesh_editing.md Phase 2).
// Pure data and string handling: no scene types, no JSON, no command framework, so a command layer only routes and the
// rules live (and are unit-tested) here.
#include "MeshEditOperations.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Phantom::MeshEdit {

	// How a new element selection combines with the current one.
	enum class SelectMode { Replace, Add, Toggle };
	bool selectModeFromName(const std::string& name, SelectMode& out);  // replace|add|toggle (case-insensitive)

	// A rejected argument: `code` is the command-layer error code (invalid_argument, not_found, ...).
	struct MeshArgError {
		std::string code, message;
	};

	// Mesh edit session state (not part of the scene, history or file). `objectId` is the editable object while `editMode`;
	// `ids` are the selected elements of `kind` (ascending).
	struct MeshEditState {
		int objectId = -1;
		bool editMode = false;
		ElementKind kind = ElementKind::Vertex;
		std::vector<uint32_t> ids;
		bool xray = false;  // pick / box select also reach elements hidden behind the surface

		// Changes the element selection: `requested` is combined with the current ids per `mode`. Ids of another element type
		// are meaningless, so switching the element type starts a fresh selection first. `clear` empties the selection;
		// `explicitIds` tells Replace that the (possibly empty) request is deliberate rather than "no ids given".
		void applySelection(ElementKind newKind, const std::vector<uint32_t>& requested, SelectMode mode, bool clear, bool explicitIds);
	};

	// The ids= argument of `mesh select`: a comma list of ids and ranges ("0,1,4-6"), "all", or "none" / empty (= clear).
	// Every id must exist in `mesh`; otherwise false with the reason in `error`.
	struct ElementIdRequest {
		std::vector<uint32_t> ids;
		bool clear = false;
	};
	bool parseElementIds(const std::string& text, const EditableMesh& mesh, ElementKind kind, ElementIdRequest& out, MeshArgError& error);

	// "x,y" or "x0,y0,x1,y1" pixel coordinates (`mesh pick` / `mesh box`): exactly `count` finite numbers.
	bool parseNumberList(const std::string& text, size_t count, std::vector<double>& out, MeshArgError& error);

	// The axis argument of a mirror modifier: "x", "z", "xy", "x,y" ... (letters x, y, z; separators ignored; empty = x).
	bool parseMirrorAxes(const std::string& text, std::array<bool, 3>& out, MeshArgError& error);

}  // namespace Phantom::MeshEdit
