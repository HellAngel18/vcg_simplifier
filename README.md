# VCG Simplifier Unreal Plugin

This plugin integrates VCGLib's quadric error metrics simplification into Unreal Engine as a LOD generator.

## Installation

1. Copy the `VCGSimplifier` folder to your project's `Plugins` directory.
2. Ensure `vcglib` is present in `Extras/vcglib`.
3. Rebuild your project.

## Usage

Once enabled, the plugin registers itself as a mesh reduction handler. You can select it in the Project Settings under `Editor -> Mesh Simplification`.

## Implementation Details

- **Simplifier**: A wrapper around VCG's `LocalOptimization` with `TriEdgeCollapseQuadricTex`.
- **VCGMeshReduction**: Implements `IMeshReduction` to bridge Unreal's `FRawMesh` and VCG's `MyMesh`.

## Parameters instructions
| CLI Option | Parameter | Default | Description |
| :--- | :--- | :--- | :--- |
| **`-r`** | `ratio` | `0.5` | **Simplification Ratio** (0.0 - 1.0). <br>If `ratio > 0`, it takes precedence and overrides `TargetFaceCount`. |
| **`-tc`** | `TargetFaceCount` | `0` | **Target Face Count**. <br>The desired number of faces. Only effective when `ratio` is set to `0` (or disabled). |
| **`-q`** | `qualityThr` | `0.3` | **Quality Threshold**. <br>Limits the simplification error. Higher values allow more deformation; lower values preserve more detail. |
| **`-tw`** | `TextureWeight` | `1.0` | **Texture Weight**. <br>Penalty weight for texture coordinate distortion. Increase this to prevent UV stretching. |
| **`-bw`** | `BoundaryWeight` | `1.0` | **Boundary Weight**. <br>Penalty weight for collapsing boundary edges. Higher values help maintain the mesh borders. |
| **`-pb`** | `preserveBoundary` | `1` (true) | **Preserve Boundary**. <br>If set to `1`, boundary edges are strictly protected from being collapsed. |
| **`-pt`** | `preserveTopology` | `1` (true) | **Preserve Topology**. <br>If set to `1`, prevents the creation of non-manifold geometry during simplification. |