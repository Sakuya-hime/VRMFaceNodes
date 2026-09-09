#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "VRMFaceTypes.h"
#include "VRMFaceManualMapper.generated.h"
class ULiveLinkRemapAsset;

// One mapper per animation instance. Blueprint executes only in game-thread PreUpdate.
UCLASS(BlueprintType)
class VRMFACENODES_API UVFNManualMapper : public UObject
{
    GENERATED_BODY()
public:
    UVFNManualMapper();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|映射", meta=(DisplayName="面捕 · 应用内置手调映射", ToolTip="传入统一曲线节点保留的原始数据。只在游戏线程调用；纯 ARKit 输入保持原映射。每个角色创建独立映射对象。"))
    FVFNFrame MapFrame(const FVFNFrame& Frame);
    static const TArray<FName>& ExtraNames();
private:
    UPROPERTY() TSubclassOf<ULiveLinkRemapAsset> PresetClass;
    UPROPERTY(Transient) TObjectPtr<ULiveLinkRemapAsset> Remapper;
    TMap<FName,FName> Names;
};
