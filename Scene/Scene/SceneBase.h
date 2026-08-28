#pragma once

#include <list>
#include <string>
#include <typeinfo>

#include "IPresenter.h"

#include "CGLib/Math/Box3d.h"
#include "CGLib/Util/UnCopyable.h"

namespace Phantom {
	namespace Scene {

/// @brief Base class for all scene nodes in the scene graph.
///
/// SceneBase forms the nodes of a tree-structured scene graph.
/// Each node has a unique integer ID, a human-readable name,
/// a list of child nodes, and a list of presenters responsible
/// for rendering the node.
class SceneBase : private UnCopyable
{
public:
	/// @brief Construct a scene node with default ID and name.
	SceneBase();

	/// @brief Construct a scene node with the given ID.
	/// @param id Unique identifier for this node.
	explicit SceneBase(const int id);

	/// @brief Construct a scene node with the given ID and name.
	/// @param id   Unique identifier for this node.
	/// @param name Human-readable name for this node.
	SceneBase(const int id, const std::string& name);

	virtual ~SceneBase();

	/// @brief Remove all children of this node and free their memory.
	/// Equivalent to clearAll(). Children are owned by this node.
	void clear();

	/// @brief Remove all children of this node recursively and free their memory.
	/// Presenters are not deleted; they are caller-owned.
	void clearAll();

	/// @brief Set the human-readable name of this node.
	/// @param name New name.
	void setName(const std::string& name) { this->name = name; }

	/// @brief Get the human-readable name of this node.
	/// @return Current name.
	std::string getName() const { return name; }

	/// @brief Set the unique ID of this node.
	/// @param id New ID.
	void setId(const int id) { this->id = id; }

	/// @brief Get the unique ID of this node.
	/// @return Current ID.
	int getId() const { return id; }

	//void setVisible(const bool b) { this->_isVisible = b; }

	//virtual SceneType getType() const = 0;

	/// @brief Add a child node to this node.
	/// @param scene Pointer to the child node to add.
	void addScene(SceneBase* scene);

	/// @brief Find a descendant node by its unique ID.
	/// @param id ID to search for.
	/// @return Pointer to the found node, or nullptr if not found.
	SceneBase* findSceneById(int id);

	/// @brief Find a descendant node by its name.
	/// @param name Name to search for.
	/// @return Pointer to the found node, or nullptr if not found.
	SceneBase* findSceneByName(const std::string& name);

	/// @brief Find all descendant nodes whose dynamic type matches @p type.
	/// @param type Type info to match against.
	/// @return List of matching nodes.
	std::list<SceneBase*> findScenesByType(const std::type_info& type);

	/// @brief Remove a descendant node with the given ID.
	/// @param id ID of the node to remove.
	void deleteSceneById(int id);

	/// @brief Find a descendant node by ID and cast it to type @p T.
	/// @tparam T Target pointer type.
	/// @param id ID to search for.
	/// @return Casted pointer, or nullptr if not found.
	template<class T>
	T findSceneById(int id) { return static_cast<T>(findSceneById(id)); }

	/// @brief Find a descendant node by name and cast it to type @p T.
	/// @tparam T Target pointer type.
	/// @param name Name to search for.
	/// @return Casted pointer, or nullptr if not found.
	template<class T>
	T findSceneByName(const std::string& name) { return static_cast<T>(findSceneByName(name)); }

	//std::list<IScene*> findScenes(SceneType type);

	/// @brief Get the axis-aligned bounding box of the scene content.
	/// @return Bounding box; default returns a degenerate (empty) box.
	virtual Math::Box3df getBoundingBox() const { return Math::Box3df::createDegeneratedBox(); }

	/// @brief Check whether this node is the root of the scene graph.
	/// @return true if this node has no parent.
	bool isRoot() const { return parent == nullptr; }

	/// @brief Get the parent node.
	/// @return Pointer to the parent node, or nullptr if this is the root.
	SceneBase* getParent() const { return parent; }

	/// @brief Get the root node of the scene graph.
	/// @return Pointer to the topmost ancestor.
	SceneBase* getRoot();

	/// @brief Get the direct children of this node.
	/// @return List of child node pointers.
	std::list<SceneBase*> getChildren() const { return children; }

	/// @brief Check whether this node is a leaf (has no children).
	/// @return true if the children list is empty.
	bool isLeaf() const { return children.empty(); }

	/// @brief Attach a presenter to this node.
	/// @param presenter Pointer to the presenter to add.
	void addPresenter(IPresenter* presenter) { this->presenters.push_back(presenter); }

	/// @brief Get all presenters attached to this node.
	/// @return List of presenter pointers.
	std::list<IPresenter*> getPresenters() { return presenters; }

	/// @brief Set the visibility of this node.
	/// @param isVisible true to make the node visible, false to hide it.
	void setVisible(const bool isVisible) { this->isVisible_ = isVisible; }

	/// @brief Check whether this node is visible.
	/// @return true if the node is visible.
	bool isVisible() { return this->isVisible_; }

	//virtual void step() {};

protected:
	std::string name;
	int id;
	std::list<SceneBase*> children;
	std::list<IPresenter*> presenters;
	SceneBase* parent;
	bool isVisible_ = true;
};

	}
}
