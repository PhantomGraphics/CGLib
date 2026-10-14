#pragma once

#include <algorithm>
#include <vector>

namespace Phantom::Editor {

// Ordered, duplicate-free IDs. Hosts decide identity, liveness and primary selection.
template <class Id>
class SelectionSet {
public:
    const std::vector<Id>& ids() const { return ids_; }
    bool empty() const { return ids_.empty(); }
    bool contains(const Id& id) const {
        return std::find(ids_.begin(), ids_.end(), id) != ids_.end();
    }
    void clear() { ids_.clear(); }
    void replace(const std::vector<Id>& ids) { ids_.clear(); add(ids); }
    void add(const std::vector<Id>& ids) {
        for (const auto& id : ids) if (!contains(id)) ids_.push_back(id);
    }
    // Repeated IDs toggle repeatedly, preserving the existing command semantics.
    void toggle(const std::vector<Id>& ids) {
        for (const auto& id : ids) {
            const auto it = std::find(ids_.begin(), ids_.end(), id);
            if (it == ids_.end()) ids_.push_back(id);
            else ids_.erase(it);
        }
    }
    template <class Pred>
    void prune(Pred alive) {
        ids_.erase(std::remove_if(ids_.begin(), ids_.end(),
            [&](const Id& id) { return !alive(id); }), ids_.end());
    }
private:
    std::vector<Id> ids_;
};

} // namespace Phantom::Editor
