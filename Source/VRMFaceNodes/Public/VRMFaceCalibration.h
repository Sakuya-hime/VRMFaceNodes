#pragma once
#include "CoreMinimal.h"
#include "VRMFaceTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VRMFaceCalibration.generated.h"

USTRUCT(BlueprintType)
struct VRMFACENODES_API FVFNCurveRange
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="自然表情基线")) float Neutral = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="静止噪声上界")) float RestCeiling = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="舒适最大幅度")) float Maximum = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="应用此通道")) bool bEnabled = false;
    UPROPERTY(BlueprintReadOnly, Category="校准") int32 SampleCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="校准增益上限", ClampMin="1", ClampMax="40", ToolTip="新校准按自然噪声自动确定。旧配置默认保持四倍上限。")) float MaxGain = 4.f;
    UPROPERTY(BlueprintReadOnly, Category="校准") bool bThreePointJaw = false;
    UPROPERTY(BlueprintReadOnly, Category="校准", meta=(DisplayName="普通张嘴输入")) float Typical = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="校准", meta=(DisplayName="使用极限张嘴辅助曲线")) bool bUseJawOpenExtreme = false;
};

USTRUCT(BlueprintType)
struct VRMFACENODES_API FVFNPerformerProfile
{
    GENERATED_BODY()
    // Keep the serialized default stable: older save games omit this field.
    // The guide explicitly upgrades a profile after fitting new ranges.
    UPROPERTY(BlueprintReadOnly, Category="校准") int32 SchemaVersion = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="配置名称")) FString Name;
    UPROPERTY(BlueprintReadOnly, Category="校准", meta=(DisplayName="录制主题")) FName Subject;
    UPROPERTY(BlueprintReadOnly, Category="校准") bool bMetaHuman = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="各通道范围")) TMap<FName,FVFNCurveRange> Ranges;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="头部自然朝向")) FRotator NeutralHead = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准", meta=(DisplayName="使用头部自然朝向")) bool bRecenterHead = false;
    UPROPERTY(BlueprintReadOnly, Category="校准") bool bMotionMeasured = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准") float MotionStart = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="校准") float MotionFull = 250.f;
    UPROPERTY(BlueprintReadOnly, Category="校准") int32 NeutralSamples = 0;
    UPROPERTY(BlueprintReadOnly, Category="校准") FString SavedAt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="微调", meta=(DisplayName="逐形态键微调")) TMap<FName,FVFNMorphTweak> MorphTweaks;
    UPROPERTY(BlueprintReadOnly, Category="微调") FString ModelPath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="镜像") bool bOverrideMirror=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="镜像", meta=(DisplayName="整体镜像")) bool bMirrorCapture=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="手调映射上使用个人嘴部校准", ToolTip="默认关闭：直接使用手调张嘴权重。开启后使用已录制的个人张嘴范围；不会删除原数据。")) bool bUsePersonalJawWithManual=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕设置", meta=(DisplayName="面部增强")) bool bEnhancementsEnabled=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕设置", meta=(DisplayName="说话张嘴幅度", ClampMin="0.5", ClampMax="1.5")) float SpeechAmount=1.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕设置", meta=(DisplayName="下巴联动", ClampMin="0", ClampMax="1")) float ChinStrength=.35f;
    UPROPERTY() bool bCameraCentered=false;
    UPROPERTY() bool bCameraVRoidAxes=true;
    UPROPERTY() bool bCameraHeadRadians=false;
    UPROPERTY() TMap<FName,float> NeutralGaze;
};

UCLASS()
class VRMFACENODES_API UVFNCalibrationSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() FVFNPerformerProfile Profile;
};

UCLASS()
class VRMFACENODES_API UVFNProfileCatalogSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() TMap<FName,FName> Bindings;
};

// The game thread owns persistence and previews. Anim nodes only copy value structs in PreUpdate.
UCLASS()
class VRMFACENODES_API UVFNCalibrationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    const FVFNPerformerProfile* FindProfile(FName Slot);
    int32 GetRevision() const { return Revision; }
    void SetPreview(FName Slot, const FVFNPerformerProfile& Profile);
    void ClearPreview(FName Slot);
    bool Save(FName Slot, const FVFNPerformerProfile& Profile, FString& Message);
    bool Reload(FName Slot, FString& Message);
    TArray<FName> ListProfiles() const;
    FName GetActiveSlot(FName Binding);
    bool Activate(FName Binding,FName StoredSlot,FString& Message);
    bool CopyProfile(FName From,FName To,FString& Message);
    bool RenameProfile(FName From,FName To,FString& Message);
    bool DeleteProfile(FName Slot,FString& Message);
    bool ExportProfile(FName Slot,FString& Message);
    bool ImportProfile(FName FileName,FName NewSlot,FString& Message);
private:
    TMap<FName,FVFNPerformerProfile> Profiles;
    TMap<FName,FVFNPerformerProfile> Previews;
    TSet<FName> Attempted;
    int32 Revision = 0;
    TMap<FName,FName> Bindings;
    bool bCatalogLoaded=false;
    void LoadCatalog();
    bool SaveCatalog();
};

UCLASS()
class VRMFACENODES_API UVFNCalibrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 应用个人校准", BlueprintThreadSafe))
    static FVFNFrame ApplyProfile(const FVFNFrame& Frame, const FVFNPerformerProfile& Profile);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 保存个人配置", WorldContext="WorldContextObject"))
    static bool SaveProfile(const UObject* WorldContextObject, FName Slot, const FVFNPerformerProfile& Profile, FString& Message);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 读取个人配置", WorldContext="WorldContextObject"))
    static bool LoadProfile(const UObject* WorldContextObject, FName Slot, FVFNPerformerProfile& Profile, FString& Message);
    UFUNCTION(BlueprintPure, Category="VRM 面捕|个人校准", meta=(DisplayName="面捕 · 检查个人配置", BlueprintThreadSafe))
    static bool ValidateProfile(const FVFNPerformerProfile& Profile, FString& Message);
    UFUNCTION(BlueprintPure, Category="VRM 面捕|逐形态键微调", meta=(DisplayName="面捕 · 计算形态键微调", BlueprintThreadSafe))
    static float ApplyMorphTweak(float Value,const FVFNMorphTweak& Tweak);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 列出配置", WorldContext="WorldContextObject"))
    static TArray<FName> ListProfiles(const UObject* WorldContextObject);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 切换配置", WorldContext="WorldContextObject"))
    static bool ActivateProfile(const UObject* WorldContextObject,FName Binding,FName StoredSlot,FString& Message);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 复制配置", WorldContext="WorldContextObject"))
    static bool CopyProfile(const UObject* WorldContextObject,FName From,FName To,FString& Message);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 重命名配置", WorldContext="WorldContextObject"))
    static bool RenameProfile(const UObject* WorldContextObject,FName From,FName To,FString& Message);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 删除配置", WorldContext="WorldContextObject"))
    static bool DeleteProfile(const UObject* WorldContextObject,FName Slot,FString& Message);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 导出配置JSON", WorldContext="WorldContextObject"))
    static bool ExportProfile(const UObject* WorldContextObject,FName Slot,FString& Message);
    UFUNCTION(BlueprintCallable, Category="VRM 面捕|配置管理", meta=(DisplayName="面捕 · 导入配置JSON", WorldContext="WorldContextObject"))
    static bool ImportProfile(const UObject* WorldContextObject,FName FileName,FName NewSlot,FString& Message);
    static float Percentile(TArray<float> Values, float Fraction);
    static bool FitRange(const TArray<float>& Neutral, const TArray<float>& Action, FVFNCurveRange& Out);
    static bool FitJawRange(const TArray<float>& Neutral, const TArray<float>& Typical, const TArray<float>& Maximum, bool bUseExtreme, FVFNCurveRange& Out, FString& Message);
    static float JawInput(const FVFNFrame& Frame, bool bUseExtreme);
    static bool IsValidSlot(FName Slot);
    static void UpgradeProfile(FVFNPerformerProfile& Profile);
    static FRotator RelativeCameraHead(const FRotator& Head,const FVFNPerformerProfile& Profile);
    static float SpeechResponse(float Value,float Amount);
};
