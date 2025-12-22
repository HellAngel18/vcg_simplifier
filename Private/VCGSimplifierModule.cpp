#include "VCGSimplifierModule.h"
#include "VCGMeshReduction.h"

#define LOCTEXT_NAMESPACE "FVCGSimplifierModule"

void FVCGSimplifierModule::StartupModule() { MeshReduction = new FVCGMeshReduction(); }

void FVCGSimplifierModule::ShutdownModule() {
    delete MeshReduction;
    MeshReduction = nullptr;
}

IMeshReduction *FVCGSimplifierModule::GetStaticMeshReductionInterface() { return MeshReduction; }

IMeshReduction *FVCGSimplifierModule::GetSkeletalMeshReductionInterface() {
    return nullptr; // Not implemented for skeletal mesh
}

IMeshMerging *FVCGSimplifierModule::GetMeshMergingInterface() { return nullptr; }

IMeshMerging *FVCGSimplifierModule::GetProxyLODMeshMergingInterface() { return nullptr; }

const FString FVCGSimplifierModule::GetName() { return FString("VCGSimplifier"); }

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVCGSimplifierModule, VCGSimplifier)
