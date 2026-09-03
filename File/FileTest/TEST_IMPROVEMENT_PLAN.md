# FileTest Improvement Plan

## Objective

Improve confidence in OBJ, glTF/GLB, PLY, and STL parsing and writing while
keeping tests portable and independent of developer-machine paths.

## Priority work

1. Add malformed, truncated, empty, and unsupported-input tests for each
   format.
2. Verify attributes and topology, not only object or vertex counts.
3. Add ASCII/binary and little-/big-endian cases where the format supports
   them.
4. Test optional glTF fields, external/data-URI buffers, sparse accessors,
   multiple primitives, and index component types.
5. Add write/read round trips with semantic comparisons and stable tolerances.
6. Move compact fixtures into test data and generate large/repetitive fixtures
   programmatically.
7. Assert clear failures for nonexistent files, invalid paths, and permission
   errors where portable.

## Common rules

- Resolve fixtures relative to the test source or configured test-data root.
- Do not depend on current working directory, locale, or platform separators.
- Validate error categories without overfitting exact operating-system text.
- Preserve a minimal regression fixture for every parser defect.

## Completion criteria

Each supported format has success, failure, edge-case, and round-trip coverage,
and all tests pass from CTest and by direct executable invocation.
