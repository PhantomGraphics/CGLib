# GltfViewer Shader Graph inspection

Load a model, then open **View > Material**, select its material index and use
**Load .phmat...**. Click **Show Shader Graph** to inspect the source graph.
The panel can also be opened from **Window > Shader Graph**.

The inspector displays node IDs, node kinds, output types, constant values,
texture slots, math operations, custom shader paths and named input connections.
Optional PBR inputs without a connection are marked `(default)`.

- Middle-button drag pans the graph; the mouse wheel zooms around the cursor.
- Click a node to inspect its full details, including custom input types.
- **Fit Graph** restores the automatic dependency layout to the visible canvas.
- Change **Material Index** to inspect another material's most recent load attempt.

The graph is read-only. Pan, zoom and selection do not change `.phmat` files.
Diagnostics from loading, validation, compilation and pipeline creation appear
above the canvas. A failed load shows the attempted source while rendering keeps
the previous shader. Clearing a material or loading a new model removes the
associated inspection snapshot. Successful hot reloads refresh the graph.

Only `.phmat` source graphs are inspected; the default PBR shader and arbitrary
GLSL/SPIR-V are not reconstructed into nodes.

Scenario command: `ShowShaderGraph:<materialIndex>` opens the panel. The
`phmat_graph_inspection.json` scenario covers valid graphs, rejected custom
shaders, preservation of the previous override, screenshots and clearing.
