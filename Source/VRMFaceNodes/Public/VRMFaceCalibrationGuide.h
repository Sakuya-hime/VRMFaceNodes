#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VRMFaceCalibration.h"
#include "VRMFaceCalibrationGuide.generated.h"
class SWidget;
class SWindow;
class USoundWaveProcedural;
class UAudioComponent;
class USkeletalMesh;
template<typename OptionType> class SComboBox;

USTRUCT(BlueprintType)
struct VRMFACENODES_API FVFNCalibrationStep
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="校准") FString Title;
    UPROPERTY(BlueprintReadOnly, Category="校准") FString Instruction;
    UPROPERTY(BlueprintReadOnly, Category="校准") TArray<FName> Channels;
    UPROPERTY(BlueprintReadOnly, Category="校准") float Seconds=5.f;
    UPROPERTY(BlueprintReadOnly, Category="校准") int32 RequiredChannels=1;
};

UCLASS(Blueprintable, meta=(DisplayName="面捕 · 引导式个人校准"))
class VRMFACENODES_API AVRMFaceCalibrationGuide : public AActor
{
    GENERATED_BODY()
public:
    AVRMFaceCalibrationGuide();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="采集主题")) FName Subject=TEXT("konohana");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="个人配置名称", ToolTip="与自动面捕或输入节点上的个人配置名称一致。保存到 Saved/SaveGames。")) FName ProfileSlot=TEXT("坐播校准");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="播放换动作提示音")) bool bPromptSound=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="启用独立配置窗口", ToolTip="运行后通过工具 → VRM面捕 · 运行配置窗口打开。配置不加入游戏画面，也不占用快捷键。")) bool bShowInterface=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="自动读取实时连接", ToolTip="默认自动读取主题。自定义输入流程可关闭此项，改用「输入校准采样」节点，每个新数据帧调用一次。")) bool bAutomaticInput=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="微调", meta=(DisplayName="微调模型", ToolTip="只有一种模型时自动识别。多模型场景请选择要微调的模型，列表会展示其全部形态键。")) TObjectPtr<USkeletalMesh> TuningModel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="对比展示", meta=(DisplayName="显示双模型标题")) bool bComparisonLabels=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="对比展示", meta=(DisplayName="左侧标题")) FString LeftTitle=TEXT("古法手搓");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="对比展示", meta=(DisplayName="右侧标题")) FString RightTitle=TEXT("VRM4U插件新功能");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="对比展示", meta=(DisplayName="右侧副标题")) FString RightSubtitle=TEXT("（全程GPT6-Astra来调试）");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="对比展示", meta=(DisplayName="右侧补充说明", MultiLine=true)) FString RightNote;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="对比展示") bool bThreeWayComparison=false;
    UPROPERTY(BlueprintReadOnly, Category="面捕设置") bool bCenteringCamera=false;
    UFUNCTION(BlueprintCallable, Category="VRM 面捕", meta=(DisplayName="面捕 · 五步校准")) void StartSimpleCalibration();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕", meta=(DisplayName="面捕 · 校正摄像头")) void CenterCamera();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕", meta=(DisplayName="面捕 · 增强开关")) void SetEnhancementsEnabled(bool Enabled);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕", meta=(DisplayName="面捕 · 调整张嘴和下巴")) void SetExpressionAmount(bool Chin,float Amount);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕", meta=(DisplayName="面捕 · 恢复原下巴")) void RestoreOriginalChin();
    UPROPERTY(BlueprintReadOnly, Category="校准") FVFNPerformerProfile Candidate;
    UPROPERTY(BlueprintReadOnly, Category="校准") FString Status;
    UPROPERTY(BlueprintReadOnly, Category="校准") FString CalibrationDetails;
    UPROPERTY(BlueprintReadOnly, Category="校准") int32 StepIndex=-1;
    UPROPERTY(BlueprintReadOnly, Category="校准") bool bRunning=false;
    UPROPERTY(BlueprintReadOnly, Category="校准") bool bComplete=false;
    UPROPERTY(BlueprintReadOnly, Category="校准") bool bPreviewing=false;
    UPROPERTY(BlueprintReadOnly, Category="校准") bool bFreshInput=false;
    UPROPERTY(BlueprintReadOnly, Category="校准") float Progress=0.f;
    UPROPERTY(BlueprintReadOnly, Category="校准") int32 JawPhase=0;

    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 开始引导校准")) void StartCalibration();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 四步快速校准", ToolTip="自然表情、闭眼、张嘴、抬眉。完成后仅保留这三类表情的个人范围，其余恢复已有自动映射；预览满意后再保存。")) void StartQuickCalibration();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 预览主要校准", ToolTip="无需重录，预览当前配置仅保留闭眼、嘴巴开合和抬眉范围的效果。取消可恢复，保存前不写入存档。")) void PreviewEssentialCalibration();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 单项快速校准", ToolTip="只采自然表情和指定动作，保留其他校准与微调。动作编号为获取动作提示列表中的索引（1 到 16）。")) void StartSingleStep(int32 ActionIndex);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 只校准嘴部", ToolTip="自然闭嘴、普通张嘴、最大张嘴，约20秒；保留眉眼、头部和其他配置。")) void StartJawCalibration(){StartSingleStep(1);}
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|逐形态键微调", meta=(DisplayName="面捕 · 设置单个形态键微调")) bool SetMorphTweak(FName Morph,const FVFNMorphTweak& Tweak);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|逐形态键微调", meta=(DisplayName="面捕 · 还原单个形态键微调")) void ResetMorphTweak(FName Morph);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 新建配置")) bool CreateProfile(const FString& Name);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 切换向导配置")) bool SelectProfile(FName Name);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 在手调映射上使用个人嘴部校准")) void SetPersonalJawWithManual(bool Enabled);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|镜像", meta=(DisplayName="面捕 · 设置整体镜像")) void SetMirrorCapture(bool Enabled);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 重做当前动作")) void RetryStep();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 跳过当前动作")) void SkipStep();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 预览校准效果")) void TogglePreview();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 保存本次校准")) bool SaveCalibration();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 读取已存校准")) bool LoadCalibration();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 取消本次校准")) void CancelCalibration();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 显示或隐藏独立配置窗口")) void TogglePanel();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 打开独立配置窗口")) void OpenConfigurationWindow();
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 输入校准采样", ToolTip="用于自定义输入。输入统一后的表情帧与帧间隔；需关闭自动读取实时连接，且提供不断更新的源时间戳。")) void FeedFrame(const FVFNFrame& Frame,float DeltaSeconds);
    UFUNCTION(BlueprintPure, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 获取动作提示")) FString GetPrompt() const;
    UFUNCTION(BlueprintPure, Category="VRM 面捕|个人校准") TArray<FVFNCalibrationStep> GetSteps() const { return Steps; }
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    TArray<FVFNCalibrationStep> Steps;
    TMap<FName,TArray<float>> NeutralSamples,ActionSamples;
    TArray<float> JawNeutralSamples,JawTypicalSamples;
    bool bJawInputChosen=false,bJawUseExtreme=false,bJawInputChanged=false;
    TArray<FRotator> NeutralHeads;
    TArray<float> MotionSpeeds;
    TArray<FString> StepResults;
    float FeedbackElapsed=0.f;
    FVFNFrame CurrentFrame;
    FRotator PreviousMotionHead=FRotator::ZeroRotator;
    double LastTimestamp=-1.;
    float StaleSeconds=1.f,Elapsed=0.f,PrepareSeconds=2.f,PendingSampleSeconds=0.f;
    bool bPanelOpen=false,bHasMotionHead=false,bCalibrationWindowPaused=false;
    bool bSingleStep=false,bPendingDelete=false,bQuickCalibration=false,bSessionWasPreviewing=false;
    TArray<int32> QuickSteps={0,2,1,4};
    int32 SingleTarget=-1,SelectedAction=2,Page=0;
    FVFNPerformerProfile SessionBefore;
    FName StoredSlot,SelectedProfile,SelectedMorph;
    FString NewProfileName,MorphFilter;
    TArray<TSharedPtr<FString>> ProfileOptions,MorphOptions;
    TArray<TSharedPtr<int32>> StepOptions;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ProfilePicker,MorphPicker;
    FName SessionSlot,SessionSubject;
    TSharedPtr<SWidget> Interface;
    TSharedPtr<SWindow> ConfigurationWindow;
    UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> Tone;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> ToneAudio;
    FTimerHandle ToneTimer;
    bool bAutoApplySession=false,bAutoSavePending=false;
    float AutoSaveDelay=0.f,CameraSeconds=0.f,CameraWallSeconds=0.f;
    TArray<FRotator> CameraHeads;
    TMap<FName,TArray<float>> CameraGaze;
    FName CameraSubject;
    bool bCameraMetaHuman=false;
    void QueueAutoSave();
    void FeedCamera(const FVFNFrame& Frame,float Dt,bool NewFrame);
    void BuildSteps();
    TArray<FName> GetActiveChannels() const;
    void KeepEssentialRanges();
    void BeginStep(int32 Index);
    void AdvanceStep();
    bool FinishStep();
    void UpdateCalibrationDetails();
    void Signal();
    void BuildInterface();
    TSharedRef<SWidget> BuildConfigurationContent();
    void CloseConfigurationWindow(bool bDestroy=false);
    TSharedRef<SWidget> BuildCalibrationPage();
    TSharedRef<SWidget> BuildTweaksPage();
    TSharedRef<SWidget> BuildProfilesPage();
    void RefreshProfiles();
    void RefreshMorphs();
    FVFNMorphTweak GetSelectedTweak() const;
    void PreviewCandidate();
    void ManageProfile(int32 Action);
    UVFNCalibrationSubsystem* Service() const;
};
