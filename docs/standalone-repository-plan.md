# CGLib Standalone Repository Plan

> Status: the standalone top-level build, presets, dependency options, tests,
> examples, and viewer gates are implemented. Installable exported targets and
> an external `find_package` consumer remain outstanding. Keep this document
> only until those packaging tasks are complete.

## Goal

Make CGLib independently clonable, configurable, buildable, testable, and
installable while preserving its use as a Phantom submodule.

## Current constraints

- Public headers, exported targets, and install rules are not yet a stable
  installed consumer interface.

## Implementation phases

1. **Complete:** establish Windows and Linux configure/build/test baselines.
2. **Complete:** move CMake helpers and presets into CGLib and remove
   parent-relative assumptions.
3. **Complete:** centralize dependency options and keep Vulkan optional.
4. **Mostly complete:** define consistent targets and usage requirements.
5. **Complete:** register tests with CTest and gate viewers behind options.
6. **Outstanding:** add install rules, exported targets, package configuration, and an external
   consumer test.
7. **Complete:** update Phantom integration without duplicate target setup.

## Completion criteria

- A clean clone builds without the Phantom parent repository.
- CPU-only builds require no Vulkan SDK.
- Enabled tests pass on supported Windows and Linux toolchains.
- An external project consumes installed targets through `find_package`.
- Phantom continues to build CGLib as a pinned submodule.
