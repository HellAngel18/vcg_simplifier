#pragma once

#include "CoreMinimal.h"
#include "IMeshReductionInterfaces.h"
#include "Modules/ModuleManager.h"

class FVCGSimplifierModule : public IMeshReductionModule {
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

  private:
    class FVCGMeshReduction *MeshReduction = nullptr; // 添加 = nullptr
};
