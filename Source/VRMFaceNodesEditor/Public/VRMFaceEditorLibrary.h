#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VRMFaceTypes.h"
#include "VRMFaceEditorLibrary.generated.h"
class USkeletalMesh;
class UAnimBlueprint;
class UPoseAsset;

USTRUCT(BlueprintType)
struct VRMFACENODESEDITOR_API FVFNSetupResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="结果") bool bSuccess=false;
    UPROPERTY(BlueprintReadOnly, Category="结果") TObjectPtr<USkeletalMesh> Model=nullptr;
    UPROPERTY(BlueprintReadOnly, Category="结果") TObjectPtr<UAnimBlueprint> AnimationBlueprint=nullptr;
    UPROPERTY(BlueprintReadOnly, Category="结果") TObjectPtr<UAnimBlueprint> PhysicsBlueprint=nullptr;
    UPROPERTY(BlueprintReadOnly, Category="结果") FVFNModelProfile Profile;
    UPROPERTY(BlueprintReadOnly, Category="结果") TArray<FString> Notes;
};

UCLASS()
class VRMFACENODESEDITOR_API UVRMFaceEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|编辑器配置", meta=(DisplayName="面捕 · 自动配置模型副本", Keywords="VRM Face Setup Model", DevelopmentOnly))
    static FVFNSetupResult ConfigureModel(USkeletalMesh* SourceModel, const FString& DestinationFolder, FName Subject, bool bAddVerifiedTeethCorrectives=true);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|编辑器配置", meta=(DisplayName="面捕 · 为已验证模型创建牙齿修正", ToolTip="仅当所有校验顶点与内置已验证配置一致时生成；不匹配则安全跳过。作用于传入的模型，请使用副本。", DevelopmentOnly))
    static bool AddVerifiedTeethCorrectives(USkeletalMesh* Model, FString& Message);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|编辑器配置", meta=(DisplayName="面捕 · 创建已验证下巴联动", DevelopmentOnly))
    static bool AddVerifiedChinCorrective(USkeletalMesh* Model,FString& Message);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|编辑器配置", meta=(DevelopmentOnly))
    static UAnimBlueprint* CreateRawComparison(USkeletalMesh* Model,UPoseAsset* ImportedPose,const FString& Folder,FName Subject);
};
