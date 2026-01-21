#include "VCGMeshReduction.h"
#include "simplifier.h"
#include "mymesh.h"

// UE5 Headers for MeshDescription
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

void FVCGMeshReduction::ReduceMeshDescription(
    FMeshDescription& OutReducedMesh,
    float& OutMaxDeviation,
    const FMeshDescription& InMesh,
    const FOverlappingCorners& InOverlappingCorners,
    const struct FMeshReductionSettings& ReductionSettings
)
{
    MyMesh m;

    // 1. Convert FMeshDescription to MyMesh
    FStaticMeshConstAttributes InAttributes(InMesh);
    TVertexAttributesConstRef<FVector3f> InVertexPositions = InAttributes.GetVertexPositions();
    TVertexInstanceAttributesConstRef<FVector2f> InVertexInstanceUVs = InAttributes.GetVertexInstanceUVs();

    // Add Vertices
    // Note: FMeshDescription Vertices can be sparse.
    // We iterate available vertices.
    
    vcg::tri::Allocator<MyMesh>::AddVertices(m, InMesh.Vertices().Num());
    
    TMap<FVertexID, int32> VertexIDToVCGIndex;
    int32 VCGVertIdx = 0;
    
    for (const FVertexID VertexID : InMesh.Vertices().GetElementIDs())
    {
        const FVector3f& Pos = InVertexPositions[VertexID];
        m.vert[VCGVertIdx].P() = vcg::Point3f(Pos.X, Pos.Y, Pos.Z);
        VertexIDToVCGIndex.Add(VertexID, VCGVertIdx);
        VCGVertIdx++;
    }

    // Add Faces (Triangles)
    vcg::tri::Allocator<MyMesh>::AddFaces(m, InMesh.Triangles().Num());
    int32 VCGFaceIdx = 0;

    for (const FTriangleID TriangleID : InMesh.Triangles().GetElementIDs())
    {
        TArrayView<const FVertexInstanceID> VertexInstances = InMesh.GetTriangleVertexInstances(TriangleID);
        if (VertexInstances.Num() != 3) continue; // Should be triangle

        bool bHasMissingVertex = false;

        for (int j = 0; j < 3; ++j)
        {
            FVertexInstanceID InstanceID = VertexInstances[j];
            FVertexID VertexID = InMesh.GetVertexInstanceVertex(InstanceID);
            
            if (int32* IdxPtr = VertexIDToVCGIndex.Find(VertexID))
            {
                m.face[VCGFaceIdx].V(j) = &m.vert[*IdxPtr];
            }
            else
            {
                 bHasMissingVertex = true;
            }

            // UVs - Channel 0
            if (InVertexInstanceUVs.GetNumChannels() > 0)
            {
                FVector2f UV = InVertexInstanceUVs.Get(InstanceID, 0); 
                m.face[VCGFaceIdx].WT(j) = vcg::TexCoord2f(UV.X, UV.Y);
            }
        }

        if (!bHasMissingVertex)
        {
            VCGFaceIdx++;
        }
    }
    
    // Resize faces vector to actual count if we skipped any
    if (VCGFaceIdx < m.face.size())
    {
        m.face.resize(VCGFaceIdx);
        m.fn = VCGFaceIdx;
    }

    // 2. Simplify
    Simplifier::Clean(m);

    Simplifier::Params params;
    params.ratio = ReductionSettings.PercentTriangles;
    // Map other settings if needed

    Simplifier::Simplify(m, params);

    // 3. Convert MyMesh back to FMeshDescription
    OutReducedMesh.Empty();
    
    FStaticMeshAttributes OutAttributes(OutReducedMesh);
    OutAttributes.Register();
    
    TVertexAttributesRef<FVector3f> OutPositions = OutAttributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector2f> OutUVs = OutAttributes.GetVertexInstanceUVs();
    TVertexInstanceAttributesRef<FVector3f> OutNormals = OutAttributes.GetVertexInstanceNormals();
    
    // Create Default PolygonGroup (Material Slot)
    FPolygonGroupID PolygonGroupID = OutReducedMesh.CreatePolygonGroup();
    
    // Reconstruct Vertices
    TMap<MyVertex*, FVertexID> VCGPtrToVertexID;
    
    for (int i = 0; i < m.vert.size(); ++i)
    {
        if (!m.vert[i].IsD())
        {
            FVertexID NewVertID = OutReducedMesh.CreateVertex();
            vcg::Point3f& P = m.vert[i].P();
            OutPositions[NewVertID] = FVector3f(P.X(), P.Y(), P.Z());
            VCGPtrToVertexID.Add(&m.vert[i], NewVertID);
        }
    }

    // Reconstruct Faces
    for (int i = 0; i < m.face.size(); ++i)
    {
        if (!m.face[i].IsD())
        {
            FVertexInstanceID CornerInstances[3];
            bool bValidFace = true;
            
            for (int j = 0; j < 3; ++j)
            {
                MyVertex* v = m.face[i].V(j);
                if (FVertexID* PID = VCGPtrToVertexID.Find(v))
                {
                    CornerInstances[j] = OutReducedMesh.CreateVertexInstance(*PID);
                    
                    // The simplifier might compact UVs or just keep them on faces.
                    // VCG stores per-face-vertex UVs in WT
                    vcg::TexCoord2f& uv = m.face[i].WT(j);
                    OutUVs.Set(CornerInstances[j], 0, FVector2f(uv.U(), uv.V())); 
                    
                    // Normal
                    // Re-use face normal for all corners (flat shading) or use VCG's logic if IsW() etc.
                    // Simplifier::Simplify computes normals.
                    vcg::Point3f& n = m.face[i].N();
                    OutNormals[CornerInstances[j]] = FVector3f(n.X(), n.Y(), n.Z());
                }
                else
                {
                    bValidFace = false;
                }
            }
            
            if (bValidFace)
            {
                OutReducedMesh.CreatePolygon(PolygonGroupID, CornerInstances);
            }
        }
    }

    OutMaxDeviation = 0.0f; 
}
