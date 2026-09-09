#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "VRMFaceTypes.h"
#include "VRMFaceCalibration.h"
#include "AnimNode_VRMFace.generated.h"
class UVFNManualMapper;

USTRUCT(BlueprintInternalUseOnly)
struct VRMFACENODES_API FAnimNode_VRMFace : public FAnimNode_Base
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="输入", meta=(DisplayName="基础姿势")) FPoseLink SourcePose;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="连接", meta=(DisplayName="Live Link 主题", PinShownByDefault, ToolTip="填写 Live Link Hub 中的 Subject 名称；留空仅在只有一个主题时自动选择。")) FName Subject;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="个人校准", meta=(DisplayName="个人配置名称", PinHiddenByDefault, ToolTip="与校准向导的配置名称一致。只在自动节点或输入节点应用；留空保留旧行为。")) FName PerformerProfileSlot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="设置", meta=(DisplayName="启用", PinHiddenByDefault)) bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="设置", meta=(DisplayName="面捕设置", PinHiddenByDefault)) FVFNSettings Settings;
    UPROPERTY() EVFNStage Stage = EVFNStage::Automatic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="测试（默认关闭）", meta=(DisplayName="使用测试帧", PinHiddenByDefault)) bool bUseTestFrame = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="测试（默认关闭）", meta=(DisplayName="测试帧", PinHiddenByDefault, EditCondition="bUseTestFrame")) FVFNFrame TestFrame;

    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
    virtual void Evaluate_AnyThread(FPoseContext& Output) override;
    virtual void GatherDebugData(FNodeDebugData& DebugData) override;
    virtual bool HasPreUpdate() const override { return true; }
    virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
private:
    UPROPERTY(Transient) TObjectPtr<UVFNManualMapper> ManualMapper;
    FVFNModelProfile Profile;
    FVFNSettings Effective;
    FVFNState State;
    FVFNFrame InputFrame;
    FVFNFrame LastLiveFrame;
    FVFNFrame SolvedFrame;
    TWeakObjectPtr<USkeletalMesh> CachedModel;
    FName CachedSubject;
    FVFNPerformerProfile PerformerProfile;
    bool bHasPerformerProfile=false;
    int32 ProfileRevision=-1;
    FName CachedProfileSlot;
    bool bModelReady=false;
    bool bNeedsStep=true;
    float DeltaSeconds=1.f/60.f;
    float LostSeconds=0.f;
    double LastTimestamp=0.;
    float RepeatedFrameSeconds=0.f;
    FVFNFrame ReadPose(const FPoseContext& Pose) const;
    void WritePose(FPoseContext& Pose, const FVFNFrame& Frame) const;
    void ApplyBones(FPoseContext& Pose, const FVFNFrame& Frame);
};
