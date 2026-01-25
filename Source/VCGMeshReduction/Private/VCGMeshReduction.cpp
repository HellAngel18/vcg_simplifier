#include "Features/IModularFeatures.h"
#include "IMeshReductionInterfaces.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
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

    void ConvertToVCGMesh(const FMeshDescription &InMesh, MyMesh &OutMesh) {
        OutMesh.Clear();
        UE_LOG(LogVCGMeshReduction, Log,
               TEXT("ConvertToVCGMesh - Start. Input Vertices: %d, Triangles: %d"),
               InMesh.Vertices().Num(), InMesh.Triangles().Num());

        FStaticMeshConstAttributes InAttributes(InMesh);
        TVertexAttributesConstRef<FVector3f> InVertexPositions = InAttributes.GetVertexPositions();
        TVertexInstanceAttributesConstRef<FVector2f> InVertexInstanceUVs =
            InAttributes.GetVertexInstanceUVs();
        TVertexInstanceAttributesConstRef<FVector4f> InVertexInstanceColors =
            InAttributes.GetVertexInstanceColors();

        // 1. Add Vertices
        // Note: FMeshDescription Vertices can be sparse.
        // We iterate available vertices.
        vcg::tri::Allocator<MyMesh>::AddVertices(OutMesh, InMesh.Vertices().Num());

        TMap<FVertexID, int32> VertexIDToVCGIndex;
        int32 VCGVertIdx = 0;

        for (const FVertexID VertexID : InMesh.Vertices().GetElementIDs()) {
            const FVector3f &Pos         = InVertexPositions[VertexID];
            OutMesh.vert[VCGVertIdx].P() = vcg::Point3f(Pos.X, Pos.Y, Pos.Z);

            // Import Vertex Color (Take average or first instance)
            if (InVertexInstanceColors.GetNumElements() > 0) {
                TArrayView<const FVertexInstanceID> VertexInstances =
                    InMesh.GetVertexVertexInstanceIDs(VertexID);
                if (VertexInstances.Num() > 0) {
                    FVector4f Color = InVertexInstanceColors[VertexInstances[0]];
                    OutMesh.vert[VCGVertIdx].C() =
                        vcg::Color4b((uint8)(Color.X * 255.f), (uint8)(Color.Y * 255.f),
                                     (uint8)(Color.Z * 255.f), (uint8)(Color.W * 255.f));
                } else {
                    OutMesh.vert[VCGVertIdx].C() = vcg::Color4b::White;
                }
            } else {
                OutMesh.vert[VCGVertIdx].C() = vcg::Color4b::White;
            }

            VertexIDToVCGIndex.Add(VertexID, VCGVertIdx);
            VCGVertIdx++;
        }

        // 2. Add Faces (Triangles)
        vcg::tri::Allocator<MyMesh>::AddFaces(OutMesh, InMesh.Triangles().Num());
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
                    OutMesh.face[VCGFaceIdx].V(j) = &OutMesh.vert[*IdxPtr];
                } else {
                    bHasMissingVertex = true;
                }

                // UVs - Channel 0
                if (InVertexInstanceUVs.GetNumChannels() > 0) {
                    FVector2f UV                   = InVertexInstanceUVs.Get(InstanceID, 0);
                    OutMesh.face[VCGFaceIdx].WT(j) = vcg::TexCoord2f(UV.X, UV.Y);
                }
            }

            // Material ID
            FPolygonGroupID GroupID        = InMesh.GetTrianglePolygonGroup(TriangleID);
            OutMesh.face[VCGFaceIdx].matId = GroupID.GetValue();

            if (!bHasMissingVertex) {
                VCGFaceIdx++;
            }
        }

        // Log unique material IDs found during conversion
        TMap<int32, int32> MatCounts;
        for (int i = 0; i < VCGFaceIdx; ++i) {
            MatCounts.FindOrAdd(OutMesh.face[i].matId)++;
        }
        UE_LOG(LogVCGMeshReduction, Log,
               TEXT("ConvertToVCGMesh - Summary: Found %d unique materials"), MatCounts.Num());
        for (const auto &Pair : MatCounts) {
            UE_LOG(LogVCGMeshReduction, Log, TEXT("  MatID: %d -> %d faces"), Pair.Key, Pair.Value);
        }

        // Resize faces vector to actual count if we skipped any
        if (VCGFaceIdx < OutMesh.face.size()) {
            OutMesh.face.resize(VCGFaceIdx);
            OutMesh.fn = VCGFaceIdx;
        }
    }

    void ConvertToFMeshDescription(const MyMesh &InVCGMesh, const FMeshDescription &OriginalMesh,
                                   FMeshDescription &OutMesh) {
        OutMesh.Empty();

        FStaticMeshAttributes OutAttributes(OutMesh);
        OutAttributes.Register();

        TVertexAttributesRef<FVector3f> OutPositions   = OutAttributes.GetVertexPositions();
        TVertexInstanceAttributesRef<FVector2f> OutUVs = OutAttributes.GetVertexInstanceUVs();
        TVertexInstanceAttributesRef<FVector3f> OutNormals =
            OutAttributes.GetVertexInstanceNormals();

        bool bHasVertexColors = OriginalMesh.VertexInstanceAttributes().HasAttribute(
            MeshAttribute::VertexInstance::Color);
        if (bHasVertexColors) {
            OutMesh.VertexInstanceAttributes().RegisterAttribute<FVector4f>(
                MeshAttribute::VertexInstance::Color);
        }
        TVertexInstanceAttributesRef<FVector4f> OutColors = OutAttributes.GetVertexInstanceColors();

        // Reconstruct PolygonGroups (Materials) from Input
        TMap<int32, FPolygonGroupID> MatIdToNewPolygonGroup;
        FStaticMeshConstAttributes InAttributes(OriginalMesh);

        TPolygonGroupAttributesConstRef<FName> InSlotNames =
            InAttributes.GetPolygonGroupMaterialSlotNames();
        TPolygonGroupAttributesRef<FName> OutSlotNames =
            OutAttributes.GetPolygonGroupMaterialSlotNames();

        UE_LOG(LogVCGMeshReduction, Log, TEXT("Reconstruction Info: HasColors: %d"),
               bHasVertexColors);

        TSet<int32> UniqueMatIds;
        for (int i = 0; i < InVCGMesh.face.size(); ++i) {
            if (!InVCGMesh.face[i].IsD()) {
                UniqueMatIds.Add(InVCGMesh.face[i].matId);
            }
        }
        UE_LOG(LogVCGMeshReduction, Log,
               TEXT("Input Mesh PolygonGroups: %d, Unique Material IDs in VCG Faces: %d"),
               OriginalMesh.PolygonGroups().Num(), UniqueMatIds.Num());

        for (const FPolygonGroupID InputGroupID : OriginalMesh.PolygonGroups().GetElementIDs()) {
            FPolygonGroupID NewGroupID = OutMesh.CreatePolygonGroup();
            MatIdToNewPolygonGroup.Add(InputGroupID.GetValue(), NewGroupID);

            OutSlotNames[NewGroupID] = InSlotNames[InputGroupID];
        }

        // Reconstruct Vertices
        TMap<const MyVertex *, FVertexID> VCGPtrToVertexID;

        for (int i = 0; i < InVCGMesh.vert.size(); ++i) {
            if (!InVCGMesh.vert[i].IsD()) {
                FVertexID NewVertID     = OutMesh.CreateVertex();
                const vcg::Point3f &P   = InVCGMesh.vert[i].P();
                OutPositions[NewVertID] = FVector3f(P.X(), P.Y(), P.Z());
                VCGPtrToVertexID.Add(&InVCGMesh.vert[i], NewVertID);
            }
        }

        // Reconstruct Faces
        for (int i = 0; i < InVCGMesh.face.size(); ++i) {
            if (!InVCGMesh.face[i].IsD()) {
                FVertexInstanceID CornerInstances[3];
                bool bValidFace = true;

                for (int j = 0; j < 3; ++j) {
                    const MyVertex *v = InVCGMesh.face[i].V(j);
                    if (FVertexID *PID = VCGPtrToVertexID.Find(v)) {
                        CornerInstances[j] = OutMesh.CreateVertexInstance(*PID);

                        // The simplifier might compact UVs or just keep them on faces.
                        // VCG stores per-face-vertex UVs in WT
                        const vcg::TexCoord2f &uv = InVCGMesh.face[i].WT(j);
                        OutUVs.Set(CornerInstances[j], 0, FVector2f(uv.U(), uv.V()));

                        // Color
                        const vcg::Color4b &c = InVCGMesh.face[i].V(j)->C();
                        if (bHasVertexColors) {
                            OutColors[CornerInstances[j]] =
                                FVector4f(c[0] / 255.f, c[1] / 255.f, c[2] / 255.f, c[3] / 255.f);
                        }

                        // Normal
                        // Use vertex normal for smooth shading
                        // 如果需要平面着色，可以使用 face[i].N()
                        const vcg::Point3f &n          = InVCGMesh.face[i].V(j)->N();
                        OutNormals[CornerInstances[j]] = FVector3f(n.X(), n.Y(), n.Z());
                    } else {
                        bValidFace = false;
                    }
                }

                if (bValidFace) {
                    FPolygonGroupID GroupID(INDEX_NONE);
                    if (FPolygonGroupID *FoundGroup =
                            MatIdToNewPolygonGroup.Find(InVCGMesh.face[i].matId)) {
                        GroupID = *FoundGroup;
                    } else if (OutMesh.PolygonGroups().Num() > 0) {
                        GroupID = *OutMesh.PolygonGroups().GetElementIDs().begin();
                    } else {
                        GroupID = OutMesh.CreatePolygonGroup();
                        MatIdToNewPolygonGroup.Add(InVCGMesh.face[i].matId, GroupID);
                    }
                    OutMesh.CreatePolygon(GroupID, CornerInstances);
                }
            }
        }
    }

    // IMeshReduction interface - UE5 Adapter

    virtual void
    ReduceMeshDescription(FMeshDescription &OutReducedMesh, float &OutMaxDeviation,
                          const FMeshDescription &InMesh,
                          const FOverlappingCorners &InOverlappingCorners,
                          const struct FMeshReductionSettings &ReductionSettings) override {
        UE_LOG(LogVCGMeshReduction, Log, TEXT("ReduceMeshDescription - Start. Target Percent: %f"),
               ReductionSettings.PercentTriangles);

        MyMesh m;

        // 1. Convert FMeshDescription to MyMesh
        ConvertToVCGMesh(InMesh, m);
        Simplifier::Params params;
        params.ratio = ReductionSettings.PercentTriangles;
        // Map Importance settings
        auto ImportanceToWeight = [](uint8 Importance) -> double {
            switch (Importance) {
            case 0:
                return 0.0; // Off
            case 1:
                return 0.25; // Lowest
            case 2:
                return 0.5; // Low
            case 3:
                return 1.0; // Normal
            case 4:
                return 3.0; // High
            case 5:
                return 10.0; // Highest
            default:
                return 1.0;
            }
        };

        params.extraTCoordWeight = ImportanceToWeight((uint8)ReductionSettings.TextureImportance);
        params.boundaryWeight = ImportanceToWeight((uint8)ReductionSettings.SilhouetteImportance);

        if ((uint8)ReductionSettings.ShadingImportance >= 4) {
            params.normalCheck = true;
        }

        Simplifier::Clean(m);

        Simplifier::Simplify(m, params);

        UE_LOG(LogVCGMeshReduction, Log,
               TEXT("Simplification Done. VCG Mesh Vertices: %d, Faces: %d"), m.vert.size(),
               m.face.size());

        // 3. Convert MyMesh back to FMeshDescription
        ConvertToFMeshDescription(m, InMesh, OutReducedMesh);

        UE_LOG(LogVCGMeshReduction, Log,
               TEXT("ReduceMeshDescription - Finished. Output Vertices: %d, Polygons: %d"),
               OutReducedMesh.Vertices().Num(), OutReducedMesh.Polygons().Num());
        // 4. Recompute Tangents
        FStaticMeshOperations::ComputeTriangleTangentsAndNormals(OutReducedMesh);

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
    IModularFeatures::Get().UnregisterModularFeature(IMeshReductionModule::GetModularFeatureName(),
                                                     this);

    GVCGMeshReduction = nullptr;
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
