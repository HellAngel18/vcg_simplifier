#include "VCGSimplifierModule.h"
#include "VCGMeshReduction.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "FVCGSimplifierModule"

void FVCGSimplifierModule::StartupModule() 
{ 
    MeshReduction = new FVCGMeshReduction(); 
    
    // Register as a modular feature so the engine can pick it up
    IModularFeatures::Get().RegisterModularFeature(IMeshReductionModule::GetModularFeatureName(), this);
}

void FVCGSimplifierModule::ShutdownModule() 
{
    IModularFeatures::Get().UnregisterModularFeature(IMeshReductionModule::GetModularFeatureName(), this);

    if (MeshReduction)
    {
        delete MeshReduction;
        MeshReduction = nullptr;
    }
}

IMeshReduction *FVCGSimplifierModule::GetStaticMeshReductionInterface() { return MeshReduction; }

IMeshReduction *FVCGSimplifierModule::GetSkeletalMeshReductionInterface() {
    return nullptr; 
}

IMeshMerging *FVCGSimplifierModule::GetMeshMergingInterface() { return nullptr; }

IMeshMerging *FVCGSimplifierModule::GetDistributedMeshMergingInterface() { return nullptr; }

FString FVCGSimplifierModule::GetName() { return FString("VCGSimplifier"); }

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVCGSimplifierModule, VCGSimplifier)
