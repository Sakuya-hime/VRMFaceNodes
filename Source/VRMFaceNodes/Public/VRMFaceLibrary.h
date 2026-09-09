#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VRMFaceTypes.h"
#include "VRMFaceLibrary.generated.h"
class USkeletalMesh;

UCLASS()
class VRMFACENODES_API UVRMFaceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="VRM 面捕|配置", meta=(DisplayName="面捕 · 分析模型", Keywords="VRM Face Inspect Auto Map"))
    static FVFNModelProfile InspectModel(USkeletalMesh* Model);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|输入", meta=(DisplayName="面捕 · 读取 Live Link", Keywords="VRM Face LiveLink"))
    static FVFNFrame ReadLiveLink(FName Subject);
    UFUNCTION(BlueprintPure, Category="VRM 面捕|映射", meta=(DisplayName="面捕 · 统一 ARKit / MetaHuman 曲线", Keywords="VRM Face Normalize Remap", BlueprintThreadSafe))
    static FVFNFrame NormalizeCurves(const TMap<FName,float>& SourceCurves);
    UFUNCTION(BlueprintPure, Category="VRM 面捕|配置", meta=(DisplayName="面捕 · 获取模型预设", BlueprintThreadSafe))
    static FVFNSettings ResolveSettings(const FVFNSettings& Settings, const FVFNModelProfile& Model);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|表情", meta=(DisplayName="面捕 · 校准闭眼与口型", BlueprintThreadSafe))
    static FVFNFrame Calibrate(const FVFNFrame& Frame, const FVFNSettings& Settings, UPARAM(ref) FVFNState& State, float DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|表情", meta=(DisplayName="面捕 · 晃头表情防抖", BlueprintThreadSafe))
    static FVFNFrame Stabilize(const FVFNFrame& Frame, const FVFNSettings& Settings, UPARAM(ref) FVFNState& State, float DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|表情", meta=(DisplayName="面捕 · 计算牙齿避让", BlueprintThreadSafe))
    static FVFNFrame RetractTeeth(const FVFNFrame& Frame, const FVFNSettings& Settings, UPARAM(ref) FVFNState& State, float DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|跟随", meta=(DisplayName="面捕 · 计算头部与上身", BlueprintThreadSafe))
    static void FollowHead(const FVFNFrame& Frame, const FVFNSettings& Settings, UPARAM(ref) FVFNState& State, float DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|状态", meta=(DisplayName="面捕 · 重置平滑状态", BlueprintThreadSafe))
    static void ResetState(UPARAM(ref) FVFNState& State);
    UFUNCTION(BlueprintPure, Category="VRM 面捕|数学", meta=(DisplayName="面捕 · 按时间平滑系数", BlueprintThreadSafe))
    static float TimeAlpha(float Seconds, float DeltaSeconds);
    // Merge after personal calibration, before mirroring. Value-only and thread safe.
    UFUNCTION(BlueprintPure, Category="VRM 面捕|映射", meta=(DisplayName="面捕 · 合入手调映射结果", BlueprintThreadSafe))
    static FVFNFrame ApplyManualBaseline(const FVFNFrame& Frame);
    // Component-space eye rotation from already calibrated/mirrored channels.
    static FRotator EyeRotation(const FVFNFrame& Frame, const FVFNSettings& Settings, bool bLeft);
    static const TArray<FName>& SemanticNames();
};
