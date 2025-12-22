#include "VCGMeshReduction.h"
#include "RawMesh.h"
#include "simplifier.h"

// We need to include VCG headers.
// Since we added the path in Build.cs, we can just include them.
#include "mymesh.h"

void FVCGMeshReduction::ReduceMesh(FRawMesh &OutReducedMesh, float &OutMaxDeviation,
                                   const FRawMesh &InMesh,
                                   const FMeshReductionSettings &ReductionSettings) {
    MyMesh m;

    // 1. Convert FRawMesh to MyMesh
    // Add vertices
    vcg::tri::Allocator<MyMesh>::AddVertices(m, InMesh.VertexPositions.Num());
    for (int i = 0; i < InMesh.VertexPositions.Num(); ++i) {
        m.vert[i].P() = vcg::Point3f(InMesh.VertexPositions[i].X, InMesh.VertexPositions[i].Y,
                                     InMesh.VertexPositions[i].Z);
    }

    // Add faces
    int numFaces = InMesh.WedgeIndices.Num() / 3;
    vcg::tri::Allocator<MyMesh>::AddFaces(m, numFaces);
    for (int i = 0; i < numFaces; ++i) {
        m.face[i].V(0) = &m.vert[InMesh.WedgeIndices[i * 3 + 0]];
        m.face[i].V(1) = &m.vert[InMesh.WedgeIndices[i * 3 + 1]];
        m.face[i].V(2) = &m.vert[InMesh.WedgeIndices[i * 3 + 2]];

        // UVs (WedgeTexCoords)
        if (InMesh.WedgeTexCoords[0].Num() > 0) {
            for (int j = 0; j < 3; ++j) {
                auto uv         = InMesh.WedgeTexCoords[0][i * 3 + j];
                m.face[i].WT(j) = vcg::TexCoord2f(uv.X, uv.Y);
            }
        }
    }

    // 2. Simplify
    Simplifier::Clean(m);

    Simplifier::Params params;
    params.ratio = ReductionSettings.PercentTriangles;
    // Map other settings if needed

    Simplifier::Simplify(m, params);

    // 3. Convert MyMesh back to FRawMesh
    OutReducedMesh.Empty();

    // Vertices
    for (auto &v : m.vert) {
        if (!v.IsD()) {
            OutReducedMesh.VertexPositions.Add(FVector3f(v.P().X(), v.P().Y(), v.P().Z()));
        }
    }

    // Faces and Wedges
    // We need to map old vertex pointers to new indices because VCG might have compacted the vector
    // Actually Simplifier::Simplify calls CompactEveryVector, so indices should be contiguous.
    for (int i = 0; i < m.face.size(); ++i) {
        if (!m.face[i].IsD()) {
            for (int j = 0; j < 3; ++j) {
                uint32 vertIndex = vcg::tri::Index(m, m.face[i].V(j));
                OutReducedMesh.WedgeIndices.Add(vertIndex);

                // UVs
                if (InMesh.WedgeTexCoords[0].Num() > 0) {
                    OutReducedMesh.WedgeTexCoords[0].Add(
                        FVector2f(m.face[i].WT(j).U(), m.face[i].WT(j).V()));
                }

                // Normals (optional, Unreal can recompute)
                OutReducedMesh.WedgeTangentZ.Add(
                    FVector3f(m.face[i].N().X(), m.face[i].N().Y(), m.face[i].N().Z()));
            }
        }
    }

    OutMaxDeviation = 0.0f; // Simplified
}
