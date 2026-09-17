#pragma once

#include "CGLib/AssetCore/AssetCore/AssetId.h"

namespace Phantom::SceneRuntime {

// A scene node's stable identifier. Same opaque UUID type as Phantom::Asset::AssetId --
// AssetId.h's own doc comment already scopes it to "an asset or entity"
// (docs/spec/phantom_asset_contract.md Sec.8 covers both under one UUID contract). Reusing
// the type rather than wrapping it again keeps "is this the same UUID Blender exported as
// phantom_uuid" a non-question once glTF node extras get bridged to scene nodes (Blender->
// Universe authoring loop Phase 2 item 5, not implemented here).
using NodeId = Phantom::Asset::AssetId;

} // namespace Phantom::SceneRuntime
