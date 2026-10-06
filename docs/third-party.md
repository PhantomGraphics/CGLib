# Third-party dependencies (PLAN_cglib_refactoring Phase 2)

All dependencies are vendored in the repository; nothing is fetched at configure time except
GoogleTest (tests only, pinned v1.15.2, never installed). CGLib itself is MIT (`LICENSE`).

"Public" means a header shipped by the installed CPU package includes it, so a consumer needs it
on the include path (the package bundles it under `include/CGLib/`, and the exported targets
carry the include directory). "Implementation" means only CGLib's own `.cpp` files use it; it
is compiled into the static library and its code is not exposed to consumers.

| Library | Version | License | Location | Role | Shipped in the install |
|---|---|---|---|---|---|
| GLM | 0.9.9.8 | MIT (or Happy Bunny) | `ThirdParty/glm-0.9.9.8` | **Public** (Math/Animation/SceneRuntime headers) | headers + `copying.txt` |
| Eigen | 3.4.0 (dev) | MPL-2.0 primarily; some files BSD/LGPL-2.1 | `Numerics/ThirdParty/eigen-3.4.0` | **Public** (`Numerics/Converter.h`) | headers + `COPYING.*` |
| nlohmann/json | 3.11.3 | MIT | `ThirdParty/nlohmann` | **Public** (AssetCore, SceneRuntime, GeometryNode headers) | header (SPDX notice inside) |
| cgltf | single header | MIT | `File/ThirdParty/cgltf` | Implementation (`File/GLTF*`); header is installed because it is in the File tree | header (notice at end of file) |
| stb_image / stb_image_write | 2.27 / see header | Public domain or MIT (dual) | `ThirdParty/stb` | Implementation (`Graphics/HDRImageFileReader`, glTF/Animation Vulkan texture loading) | not shipped |
| pugixml | 1.x (2006-2023) | MIT | `ThirdParty/pugixml` | Implementation (`PugixmlCore`, not exported) | not shipped |
| Dear ImGui | 1.92.7 | MIT | `ThirdParty/imgui` | Vulkan/UI layer only (`UIWidgetsCore`) | not in the CPU package |
| VulkanMemoryAllocator | recent (2017-2026 AMD) | MIT | `ThirdParty/VulkanMemoryAllocator` | Vulkan layer only | not in the CPU package |
| GLFW | 3.3.8 | zlib/libpng | `ThirdParty/glfw-3.3.8` | Headers for the Vulkan/viewer layer; loader is found on the system | not in the CPU package |
| tinyfiledialogs | 3.x (2014-2021) | zlib-style | `ThirdParty/tinyfiledialogs` | UI layer only | not in the CPU package |
| GoogleTest | v1.15.2 | BSD-3-Clause | fetched (FetchContent) | tests only | never |

## Rules the build follows

- A dependency that appears in an installed header is listed as **Public** above and its license
  text must be installed with it (`share/licenses/CGLib/<name>/`). `cmake/CGLibInstall.cmake`
  does this for GLM and Eigen; nlohmann and cgltf carry their notice inside the header file.
- Implementation-only libraries are statically compiled in; redistributing the resulting
  binaries still requires their notices (stb, pugixml are MIT/public domain).
- Eigen: parts of Eigen are LGPL-2.1 and BSD. Defining `EIGEN_MPL2_ONLY` before including Eigen
  restricts a build to MPL-2.0 code; CGLib's sources only use the dense/SVD parts, but the
  macro is not forced on consumers.
- Before adding a dependency: decide Public vs Implementation, add a row here, and extend the
  install rules (`CGLibInstall.cmake`) and the header self-containment check
  (`tests/run_install_consumer.ps1`) when it is Public.
