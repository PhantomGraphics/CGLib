#include "SceneBase.h"

using namespace Phantom::Scene;

SceneBase::SceneBase() :
	id(-1),
	parent(nullptr)
{}

SceneBase::SceneBase(const int id) :
	id(id),
	parent(nullptr)
{}

SceneBase::SceneBase(const int id, const std::string& name) :
	id(id),
	name(name),
	parent(nullptr)
{}

SceneBase::~SceneBase()
{
	clearAll();
}

void SceneBase::clear()
{
	clearAll();
}

void SceneBase::clearAll()
{
	for (auto c : children) {
		delete c;  // destructor recursively cleans up c's subtree
	}
	children.clear();
	// presenters are caller-owned; only remove references, do not delete
	presenters.clear();
}

void SceneBase::addScene(SceneBase* scene)
{
	scene->parent = this;
	this->children.push_back(scene);
}

SceneBase* SceneBase::findSceneById(int id)
{
	if (id == this->id) {
		return this;
	}
	for (auto c : children) {
		auto s = c->findSceneById(id);
		if (s != nullptr) {
			return s;
		}
	}
	return nullptr;
}

std::list<SceneBase*> SceneBase::findScenesByType(const std::type_info& type)
{
	std::list<SceneBase*> results;
	if (type == typeid(*this)) {
		results.push_back(this);
	}
	for (auto c : children) {
		auto s = c->findScenesByType(type);
		results.insert(results.end(), s.begin(), s.end());
	}
	return results;
}

void SceneBase::deleteSceneById(int id)
{
	SceneBase* target = findSceneById(id);
	if (!target || target == this) return;
	SceneBase* p = target->parent;
	if (!p) return;
	p->children.remove(target);
	delete target;  // destructor recursively cleans up subtree
}

SceneBase* SceneBase::findSceneByName(const std::string& name)
{
	if (name == this->name) {
		return this;
	}
	for (auto c : children) {
		auto s = c->findSceneByName(name);
		if (s != nullptr) {
			return s;
		}
	}
	return nullptr;
}

SceneBase* SceneBase::getRoot()
{
	auto p = this;
	while (!p->isRoot()) {
		p = p->getParent();
	}
	return p;
}
