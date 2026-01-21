# pragma once

#include "CoreMinimal.h"
#include "IMeshReductionInterfaces.h"
#include "MeshDescription.h"

class FVCGMeshReduction : public IMeshReduction {
  public:
    virtual ~FVCGMeshReduction() {}

    // IMeshReduction interface - UE5 Adapter
    
    virtual void ReduceMeshDescription(
        FMeshDescription& OutReducedMesh,
        float& OutMaxDeviation,
        const FMeshDescription& InMesh,
        const FOverlappingCorners& InOverlappingCorners,
        const struct FMeshReductionSettings& ReductionSettings
    ) override;

    virtual bool ReduceSkeletalMesh(
        class USkeletalMesh* SkeletalMesh,
        int32 LODIndex,
        const class ITargetPlatform* TargetPlatform
    ) override { return false; }

    virtual const FString& GetVersionString() const override
    {
        static FString Version = TEXT("VCG_1.0");
        return Version;
    }

    virtual bool IsSupported() const override { return true; }

    virtual bool IsReductionActive(const struct FMeshReductionSettings &ReductionSettings) const override
    {
        return ReductionSettings.PercentTriangles < 1.0f || ReductionSettings.MaxDeviation > 0.0f;
    }

    virtual bool IsReductionActive(const struct FMeshReductionSettings& ReductionSettings, uint32 NumVertices, uint32 NumTriangles) const override
    {
        return IsReductionActive(ReductionSettings);
    }

    virtual bool IsReductionActive(const struct FSkeletalMeshOptimizationSettings &ReductionSettings) const override
    {
        return false;
    }

    virtual bool IsReductionActive(const struct FSkeletalMeshOptimizationSettings &ReductionSettings, uint32 NumVertices, uint32 NumTriangles) const override
    {
        return false;
    }
};
