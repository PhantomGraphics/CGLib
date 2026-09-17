#include "SceneNode.h"

namespace Phantom::SceneRuntime {

bool operator==(const ComponentRecord& a, const ComponentRecord& b)
{
    return a.type == b.type && a.data == b.data;
}

} // namespace Phantom::SceneRuntime
