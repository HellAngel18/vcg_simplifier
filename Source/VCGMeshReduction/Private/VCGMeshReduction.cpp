#include "Features/IModularFeatures.h"
#include "IMeshReductionInterfaces.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "mymesh.h"
#include "simplifier.h"

class FVCGMeshReductionModule : public IMeshReductionModule {
  public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** IMeshReductionModule implementation */
    virtual class IMeshReduction *GetStaticMeshReductionInterface() override;
    virtual class IMeshReduction *GetSkeletalMeshReductionInterface() override;
    virtual class IMeshMerging *GetMeshMergingInterface() override;
    virtual class IMeshMerging *GetDistributedMeshMergingInterface() override;
    virtual FString GetName() override;
};

DEFINE_LOG_CATEGORY_STATIC(LogVCGMeshReduction, Log, All);
IMPLEMENT_MODULE(FVCGMeshReductionModule, VCGMeshReduction);

class FVCGMeshReduction : public IMeshReduction {
  public:
    virtual ~FVCGMeshReduction() {}

    // IMeshReduction interface - UE5 Adapter

    virtual void
    ReduceMeshDescription(FMeshDescription &OutReducedMesh, float &OutMaxDeviation,
                          const FMeshDescription &InMesh,
                          const FOverlappingCorners &InOverlappingCorners,
                          const struct FMeshReductionSettings &ReductionSettings) override {

        MyMesh m;

        // 1. Convert FMeshDescription to MyMesh
        FStaticMeshConstAttributes InAttributes(InMesh);
        TVertexAttributesConstRef<FVector3f> InVertexPositions = InAttributes.GetVertexPositions();
        TVertexInstanceAttributesConstRef<FVector2f> InVertexInstanceUVs =
            InAttributes.GetVertexInstanceUVs();

        // Add Vertices
        // Note: FMeshDescription Vertices can be sparse.
        // We iterate available vertices.

        vcg::tri::Allocator<MyMesh>::AddVertices(m, InMesh.Vertices().Num());

        TMap<FVertexID, int32> VertexIDToVCGIndex;
        int32 VCGVertIdx = 0;

        for (const FVertexID VertexID : InMesh.Vertices().GetElementIDs()) {
            const FVector3f &Pos   = InVertexPositions[VertexID];
            m.vert[VCGVertIdx].P() = vcg::Point3f(Pos.X, Pos.Y, Pos.Z);
            VertexIDToVCGIndex.Add(VertexID, VCGVertIdx);
            VCGVertIdx++;
        }

        // Add Faces (Triangles)
        vcg::tri::Allocator<MyMesh>::AddFaces(m, InMesh.Triangles().Num());
        int32 VCGFaceIdx = 0;

        for (const FTriangleID TriangleID : InMesh.Triangles().GetElementIDs()) {
            TArrayView<const FVertexInstanceID> VertexInstances =
                InMesh.GetTriangleVertexInstances(TriangleID);
            if (VertexInstances.Num() != 3)
                continue; // Should be triangle

            bool bHasMissingVertex = false;

            for (int j = 0; j < 3; ++j) {
                FVertexInstanceID InstanceID = VertexInstances[j];
                FVertexID VertexID           = InMesh.GetVertexInstanceVertex(InstanceID);

                if (int32 *IdxPtr = VertexIDToVCGIndex.Find(VertexID)) {
                    m.face[VCGFaceIdx].V(j) = &m.vert[*IdxPtr];
                } else {
                    bHasMissingVertex = true;
                }

                // UVs - Channel 0
                if (InVertexInstanceUVs.GetNumChannels() > 0) {
                    FVector2f UV             = InVertexInstanceUVs.Get(InstanceID, 0);
                    m.face[VCGFaceIdx].WT(j) = vcg::TexCoord2f(UV.X, UV.Y);
                }
            }

            if (!bHasMissingVertex) {
                VCGFaceIdx++;
            }
        }

        // Resize faces vector to actual count if we skipped any
        if (VCGFaceIdx < m.face.size()) {
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

        TVertexAttributesRef<FVector3f> OutPositions   = OutAttributes.GetVertexPositions();
        TVertexInstanceAttributesRef<FVector2f> OutUVs = OutAttributes.GetVertexInstanceUVs();
        TVertexInstanceAttributesRef<FVector3f> OutNormals =
            OutAttributes.GetVertexInstanceNormals();

        // Create Default PolygonGroup (Material Slot)
        FPolygonGroupID PolygonGroupID = OutReducedMesh.CreatePolygonGroup();

        // Reconstruct Vertices
        TMap<MyVertex *, FVertexID> VCGPtrToVertexID;

        for (int i = 0; i < m.vert.size(); ++i) {
            if (!m.vert[i].IsD()) {
                FVertexID NewVertID     = OutReducedMesh.CreateVertex();
                vcg::Point3f &P         = m.vert[i].P();
                OutPositions[NewVertID] = FVector3f(P.X(), P.Y(), P.Z());
                VCGPtrToVertexID.Add(&m.vert[i], NewVertID);
            }
        }

        // Reconstruct Faces
        for (int i = 0; i < m.face.size(); ++i) {
            if (!m.face[i].IsD()) {
                FVertexInstanceID CornerInstances[3];
                bool bValidFace = true;

                for (int j = 0; j < 3; ++j) {
                    MyVertex *v = m.face[i].V(j);
                    if (FVertexID *PID = VCGPtrToVertexID.Find(v)) {
                        CornerInstances[j] = OutReducedMesh.CreateVertexInstance(*PID);

                        // The simplifier might compact UVs or just keep them on faces.
                        // VCG stores per-face-vertex UVs in WT
                        vcg::TexCoord2f &uv = m.face[i].WT(j);
                        OutUVs.Set(CornerInstances[j], 0, FVector2f(uv.U(), uv.V()));

                        // Normal
                        // Re-use face normal for all corners (flat shading) or use VCG's logic if
                        // IsW() etc. Simplifier::Simplify computes normals.
                        vcg::Point3f &n                = m.face[i].N();
                        OutNormals[CornerInstances[j]] = FVector3f(n.X(), n.Y(), n.Z());
                    } else {
                        bValidFace = false;
                    }
                }

                if (bValidFace) {
                    OutReducedMesh.CreatePolygon(PolygonGroupID, CornerInstances);
                }
            }
        }

        OutMaxDeviation = 0.0f;
    }

    virtual bool ReduceSkeletalMesh(class USkeletalMesh *SkeletalMesh, int32 LODIndex,
                                    const class ITargetPlatform *TargetPlatform) override {
        return false;
    }

    virtual const FString &GetVersionString() const override {
        static FString Version = TEXT("VCG_1.0");
        return Version;
    }

    virtual bool IsSupported() const override { return true; }

    virtual bool
    IsReductionActive(const struct FMeshReductionSettings &ReductionSettings) const override {
        return ReductionSettings.PercentTriangles < 1.0f || ReductionSettings.MaxDeviation > 0.0f;
    }

    virtual bool IsReductionActive(const struct FMeshReductionSettings &ReductionSettings,
                                   uint32 NumVertices, uint32 NumTriangles) const override {
        return IsReductionActive(ReductionSettings);
    }

    virtual bool IsReductionActive(
        const struct FSkeletalMeshOptimizationSettings &ReductionSettings) const override {
        return false;
    }

    virtual bool
    IsReductionActive(const struct FSkeletalMeshOptimizationSettings &ReductionSettings,
                      uint32 NumVertices, uint32 NumTriangles) const override {
        return false;
    }

    static FVCGMeshReduction *Create() { return new FVCGMeshReduction(); }
};

TUniquePtr<FVCGMeshReduction> GVCGMeshReduction;

void FVCGMeshReductionModule::StartupModule() {
    GVCGMeshReduction.Reset(FVCGMeshReduction::Create());
    IModularFeatures::Get().RegisterModularFeature(IMeshReductionModule::GetModularFeatureName(),
                                                   this);
}

void FVCGMeshReductionModule::ShutdownModule() {
    GVCGMeshReduction = nullptr;
    IModularFeatures::Get().UnregisterModularFeature(IMeshReductionModule::GetModularFeatureName(),
                                                     this);
}

IMeshReduction *FVCGMeshReductionModule::GetStaticMeshReductionInterface() {
    return GVCGMeshReduction.Get();
}

IMeshReduction *FVCGMeshReductionModule::GetSkeletalMeshReductionInterface() { return nullptr; }

IMeshMerging *FVCGMeshReductionModule::GetMeshMergingInterface() { return nullptr; }

class IMeshMerging *FVCGMeshReductionModule::GetDistributedMeshMergingInterface() {
    return nullptr;
}

FString FVCGMeshReductionModule::GetName() { return FString("VCGMeshReduction"); }
