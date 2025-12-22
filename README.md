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
