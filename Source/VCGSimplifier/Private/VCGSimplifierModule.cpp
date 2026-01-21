#include "VCGSimplifierModule.h"
#include "Features/IModularFeatures.h"
#include "VCGMeshReduction.h"

#define LOCTEXT_NAMESPACE "FVCGSimplifierModule"
// 定义日志分类 (可选，方便调试)
DEFINE_LOG_CATEGORY_STATIC(LogVCGSimplifier, Log, All);


void FVCGSimplifierModule::StartupModule() {
    // 创建减少算法实例
    MeshReduction = new FVCGMeshReduction();

    // 注册为模块化特性，以便引擎通过 IModularFeatures 获取
    if (MeshReduction) {
        IModularFeatures::Get().RegisterModularFeature(
            IMeshReductionModule::GetModularFeatureName(), this);
        UE_LOG(LogVCGSimplifier, Log,
               TEXT("VCGSimplifier Module has been loaded and registered named: %s"),
               *IMeshReductionModule::GetModularFeatureName().ToString());
    } else {
        UE_LOG(LogVCGSimplifier, Error, TEXT("Failed to create FVCGMeshReduction instance!"));
    }
}

void FVCGSimplifierModule::ShutdownModule() {
    // 必须先注销特性，再删除实例
    IModularFeatures::Get().UnregisterModularFeature(IMeshReductionModule::GetModularFeatureName(),
                                                     this);

    if (MeshReduction) {
        delete MeshReduction;
        MeshReduction = nullptr;
    }

    UE_LOG(LogVCGSimplifier, Log, TEXT("VCGSimplifier Module has been shutdown"));
}

IMeshReduction *FVCGSimplifierModule::GetStaticMeshReductionInterface() { return MeshReduction; }

IMeshReduction *FVCGSimplifierModule::GetSkeletalMeshReductionInterface() { return nullptr; }

IMeshMerging *FVCGSimplifierModule::GetMeshMergingInterface() { return nullptr; }

IMeshMerging *FVCGSimplifierModule::GetDistributedMeshMergingInterface() { return nullptr; }

FString FVCGSimplifierModule::GetName() { return FString("VCGSimplifier"); }

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVCGSimplifierModule, VCGSimplifier)
