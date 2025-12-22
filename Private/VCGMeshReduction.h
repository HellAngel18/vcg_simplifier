#pragma once

#include "CoreMinimal.h"
#include "MeshReductionInterfaces.h"

class FVCGMeshReduction : public IMeshReduction {
  public:
    // IMeshReduction interface
    virtual void ReduceMesh(FRawMesh &OutReducedMesh, float &OutMaxDeviation,
                            const FRawMesh &InMesh,
                            const FMeshReductionSettings &ReductionSettings) override;

    virtual bool IsSupported() const override { return true; }

    // Other methods might need to be implemented depending on UE version
    // For simplicity, we focus on ReduceMesh
};
