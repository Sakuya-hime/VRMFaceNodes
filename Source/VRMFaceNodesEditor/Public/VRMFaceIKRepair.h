#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VRMFaceIKRepair.generated.h"
class UIKRetargeter;
class UIKRigDefinition;

USTRUCT(BlueprintType)
struct VRMFACENODESEDITOR_API FVFNIKRepairResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="修复") bool bSupported=false;
    UPROPERTY(BlueprintReadOnly, Category="修复") bool bNeedsRepair=false;
    UPROPERTY(BlueprintReadOnly, Category="修复") bool bSuccess=false;
    UPROPERTY(BlueprintReadOnly, Category="修复") TObjectPtr<UIKRetargeter> Retargeter;
    UPROPERTY(BlueprintReadOnly, Category="修复") TObjectPtr<UIKRigDefinition> SourceRig;
    UPROPERTY(BlueprintReadOnly, Category="修复") TArray<FString> Notes;
};
UCLASS()
class VRMFACENODESEDITOR_API UVRMFaceIKRepair : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|腿脚修复", meta=(DisplayName="面捕 · 检查腿脚链映射", DevelopmentOnly))
    static FVFNIKRepairResult Inspect(UIKRetargeter* Retargeter);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|腿脚修复", meta=(DisplayName="面捕 · 一键修复腿脚（生成副本）", DevelopmentOnly, ToolTip="检查标准 UE 源骨架到 VRoid 目标骨架的四条链。仅生成专用重定向器和源 IK 副本，保留原资产；已正确或不匹配时不重复修改。"))
    static FVFNIKRepairResult Repair(UIKRetargeter* Retargeter,const FString& DestinationFolder);
};
