# VulkanGraphics ownership and lifecycle

Scope: the classes in `VulkanGraphics/`. Rules for every class unless a row says otherwise
(enforced by `VulkanGraphicsTest/VulkanLifecycleTest.cpp`, run with the validation layer):

- `destroy()` on a never-created object, and `destroy()` twice, are no-ops.
- `create()` / `init()` on a live object releases the previous handles first (no leak).
- A failed `create()` frees everything it made and leaves the object invalid (null handles).
- Destruction is explicit; destructors do not release Vulkan handles (except `VulkanContext`).
- Nothing here waits for the GPU. The caller must ensure no in-flight command buffer uses a
  handle before `destroy()` / `create()` / `resize()` replaces it.

| Class | Owns | Non-owning references | Release order / notes |
|---|---|---|---|
| `VulkanContext` | instance, debug messenger, physical device selection, device, VMA allocator | surface (caller creates/destroys) | Destructor calls `destroy()`: allocator → device → messenger → instance. Destroy the surface *before* `ctx.destroy()`. Everything created from the device must already be destroyed. `createInstance()` on a live context destroys it first. |
| `VulkanCommandPool` | `VkCommandPool` | `VulkanContext*` | Command buffers from it are freed with the pool. Pool must outlive buffers/uploads using it. |
| `VulkanBuffer` | VMA buffer + allocation (mapped pointer valid until `destroy()`) | VMA allocator (cached from ctx) | Movable, not copyable; `operator=(&&)` destroys the target first. Must be destroyed before the context. |
| `VulkanImage` (static helpers) | nothing — caller owns the returned image/memory/view | — | Destroy view, then image, then free memory. Failure paths free partial objects and null the outputs. |
| `VulkanSampler` | `VkSampler` | — | Standalone. |
| `VulkanCubeMap` | image, memory, view, sampler | — | Standalone; `swap()` exchanges ownership. |
| `VulkanDescriptorSetLayout` | layout | — | May be destroyed once pipelines/sets using it are created (but not while sets are being allocated). |
| `VulkanDescriptorPool` | pool (allocated sets die with it) | — | Destroy the pool, not the sets; sets must not be in use. |
| `VulkanRenderPass` | `VkRenderPass` | — | Destroy pipelines/framebuffers built from it first. |
| `VulkanPipeline` / `VulkanComputePipeline` | pipeline + pipeline layout (shader modules are transient) | render pass; descriptor set layouts passed in the config | Destroy before the render pass. Pipeline must not be in use by an in-flight command buffer. |
| `VulkanOffscreen` | color/depth images + memory + views, its own render pass, framebuffer | — | `create()` rebuilds everything (render pass handle changes); `resize()` keeps the render pass handle and rebuilds only images + framebuffer. |
| `VulkanSwapChain` | swapchain, image views, MSAA color, depth, framebuffers | `VulkanContext*`, surface, caller's render pass | `init()` must precede use; `destroy()` before `init()` is a no-op. `recreate()` waits idle, destroys, recreates. The render pass is the caller's and must outlive the framebuffers. |

Higher layers (`VkAppBase`, `GltfRenderer`, `VolumeRenderer`, `AnimationRenderer`, ...) hold these by value
or `unique_ptr` and destroy them in `onDestroy`/`destroy(device)` in reverse creation order; their ownership
tables are a Phase 4 item.
