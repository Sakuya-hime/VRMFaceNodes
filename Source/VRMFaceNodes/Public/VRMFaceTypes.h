#pragma once
#include "CoreMinimal.h"
#include "VRMFaceTypes.generated.h"

UENUM(BlueprintType)
enum class EVFNStage : uint8
{
    Automatic, Input, Calibration, Stabilization, HeadBody, Eyes, Teeth
};

USTRUCT(BlueprintType)
struct VRMFACENODES_API FVFNModelProfile
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="模型", meta=(DisplayName="识别为 VRoid 骨架")) bool bVRoid = false;
    UPROPERTY(BlueprintReadOnly, Category="模型", meta=(DisplayName="已匹配表情数")) int32 MatchedCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="模型", meta=(DisplayName="表情映射")) TMap<FName,FName> MorphMap;
    UPROPERTY(BlueprintReadOnly, Category="模型", meta=(DisplayName="未找到的表情")) TArray<FName> MissingCurves;
    UPROPERTY(BlueprintReadOnly, Category="模型", meta=(DisplayName="说明")) TArray<FString> Notes;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName Head;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName Chest;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName LeftEye;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName RightEye;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName LeftUpperArm;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName RightUpperArm;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName TeethMorph;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName MouthInnerMorph;
    UPROPERTY(BlueprintReadOnly, Category="模型") FName ChinMorph;
};

USTRUCT(BlueprintType)
struct VRMFACENODES_API FVFNMorphTweak
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="微调", meta=(DisplayName="启用此形态键")) bool bEnabled=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="微调", meta=(DisplayName="幅度倍率", ClampMin="0", ClampMax="3")) float Gain=1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="微调", meta=(DisplayName="基线偏移", ClampMin="-1", ClampMax="1")) float Offset=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="微调", meta=(DisplayName="输出下限", ClampMin="0", ClampMax="1.5")) float Minimum=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="微调", meta=(DisplayName="输出上限", ClampMin="0", ClampMax="1.5")) float Maximum=1.5f;
};

USTRUCT(BlueprintType)
struct VRMFACENODES_API FVFNSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="设置", meta=(DisplayName="面部增强总开关")) bool bEnhancementsEnabled=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="下巴联动强度", ClampMin="0", ClampMax="1")) float ChinStrength=.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="说话张嘴幅度", ClampMin="0.5", ClampMax="1.5")) float SpeechAmount=1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="预设", meta=(DisplayName="使用内置手调映射", ToolTip="MetaHuman 输入使用作者手调的命名映射与宏算法，再应用个人眉眼校准、镜像、防抖和牙齿避让。关闭可恢复1.2.8的映射。")) bool bUseManualMapping=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="预设", meta=(DisplayName="自动使用模型预设", ToolTip="VRoid 完美同步骨架使用本次调校的起始参数；其他骨架使用中性表情参数。关闭后完全使用下方数值。")) bool bAutomaticPreset = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="预设", meta=(DisplayName="启用表情校准")) bool bCalibration = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="预设", meta=(DisplayName="前置摄像头镜像（左右翻转，点头不变）", ToolTip="头部、左右眉毛、眼睑、嘴角和视线保持同一镜像方向；上下点头不反转。VRoid 预设开启时沿用手动版头部方向。替代旧的仅交换左右眼选项，避免重复反转。")) bool bMirrorCapture = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="预设", meta=(DisplayName="交换左右眼输入")) bool bSwapEyes = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="预设", meta=(DisplayName="使用 VRoid 头部轴向")) bool bVRoidAxes = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="预设", meta=(DisplayName="头部输入为弧度")) bool bHeadRadians = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闭眼", meta=(DisplayName="闭眼输入起点", ClampMin="0", ClampMax="1")) float BlinkStart = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闭眼", meta=(DisplayName="闭眼输入满幅", ClampMin="0.001", ClampMax="1")) float BlinkEnd = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闭眼", meta=(DisplayName="闭眼最终幅度", ClampMin="0", ClampMax="1.5")) float BlinkGain = 1.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闭眼", meta=(DisplayName="闭眼响应速度", ClampMin="0.01")) float BlinkCloseRate = 38.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闭眼", meta=(DisplayName="睁眼响应速度", ClampMin="0.01")) float BlinkOpenRate = 22.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="嘬嘴输入上限", ClampMin="0", ClampMax="1")) float PuckerLimit = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="微笑嘴角幅度", ClampMin="0", ClampMax="1.5", ToolTip="本视频调校方案：左右 MouthSmile 为原幅度的 70%。形态键微调在此之后叠加。")) float SmileGain = .7f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="额外闭嘴强度", ClampMin="0", ClampMax="1")) float MouthCloseStrength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="自然闭嘴输入死区", ClampMin="0", ClampMax="0.3", ToolTip="去除 JawOpen 的低幅残余输入，随后连续重映射剩余范围。个人校准已覆盖张嘴通道时不重复应用。0 保留旧行为。")) float JawRestThreshold = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="上唇内卷起点", ClampMin="0", ClampMax="1")) float LipRollStart = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="上唇内卷满幅", ClampMin="0.001", ClampMax="1")) float LipRollEnd = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="上唇内卷最大幅度", ClampMin="0", ClampMax="1")) float LipRollLimit = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="口型", meta=(DisplayName="VRoid 嘬嘴柔和响应")) bool bPuckerSmoothstep = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="启用晃头防抖")) bool bStabilize = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="快速晃头时过滤眉眼突刺", ToolTip="以最近三帧的中值过滤短暂眉眼误捕，持续变化只增加约一帧延迟，不再保持到头部停稳。关闭后仅平滑。")) bool bHoldMotionExpressions = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="晃头平滑时间上限", Units="s", ClampMin="0", ClampMax="0.2", ToolTip="限制晃头时嘴部、眉毛和眼睑的平滑时间常数，默认 0.04 秒；越小越跟手，越大越柔和。眨眼最多使用此值的一半。它不是总输入延迟，静止平滑不受影响。")) float MotionResponseLimit = .04f;
    // Retained only for loading 1.2.4 assets. The unbounded hold / blink gate was removed.
    UPROPERTY() float BlinkConfirmSeconds = .06f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="晃头触发角速度", Units="deg/s", ClampMin="0")) float MotionStart = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="晃头满幅角速度", Units="deg/s", ClampMin="1")) float MotionFull = 250.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="停稳恢复时间", Units="s", ClampMin="0.001")) float RecoverySeconds = .16f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="嘴部静止平滑", Units="s", ClampMin="0")) float MouthStill = .015f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="嘴部晃头平滑", Units="s", ClampMin="0")) float MouthMoving = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="眉毛静止平滑", Units="s", ClampMin="0")) float BrowStill = .025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="眉毛晃头平滑", Units="s", ClampMin="0")) float BrowMoving = .20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="眼睑静止平滑", Units="s", ClampMin="0")) float LidStill = .02f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="眼睑晃头平滑", Units="s", ClampMin="0")) float LidMoving = .20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="眨眼晃头平滑", Units="s", ClampMin="0")) float BlinkMoving = .18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="防抖", meta=(DisplayName="主动闭眼响应", Units="s", ClampMin="0")) float IntentionalBlinkSeconds = .02f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="跟随", meta=(DisplayName="启用头部跟随")) bool bHead = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="跟随", meta=(DisplayName="头部灵敏度", ClampMin="0", ClampMax="2")) float HeadGain = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="跟随", meta=(DisplayName="头部平滑速度", ClampMin="0")) float HeadRate = 14.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="跟随", meta=(DisplayName="上身跟随幅度", ClampMin="0", ClampMax="1")) float BodyGain = .32f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="跟随", meta=(DisplayName="上身平滑速度", ClampMin="0")) float BodyRate = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="跟随", meta=(DisplayName="坐播手臂放松", ToolTip="仅对已识别的 VRoid 骨架添加放松角度；叠加已有身体动画时可关闭。")) bool bRelaxArms = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="眼球", meta=(DisplayName="启用八向眼球")) bool bEyes = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="眼球", meta=(DisplayName="眼球最大角度", Units="deg", ClampMin="0", ClampMax="40")) float EyeDegrees = 15.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="牙齿", meta=(DisplayName="启用牙齿避让")) bool bTeeth = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="牙齿", meta=(DisplayName="牙齿立即避让", ToolTip="增加避让时同帧响应；回正时平滑复位。避让不再被嘴部防抖重复减速。")) bool bInstantTeeth = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="牙齿", meta=(DisplayName="歪嘴后退起点", ClampMin="0", ClampMax="1")) float TeethStart = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="牙齿", meta=(DisplayName="歪嘴后退满幅", ClampMin="0.001", ClampMax="1")) float TeethEnd = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="牙齿", meta=(DisplayName="牙齿后退最大权重", ClampMin="0", ClampMax="1", ToolTip="对应已制作的避让形态键；本模型权重 1 等于 1 厘米。其他模型的单位由其形态键决定。")) float TeethAmount = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="牙齿", meta=(DisplayName="牙齿响应速度", ClampMin="0")) float TeethRate = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="连接", meta=(DisplayName="断流保留时间", Units="s", ClampMin="0")) float LostHoldSeconds = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="连接", meta=(DisplayName="断流回正时间", Units="s", ClampMin="0.01")) float LostFadeSeconds = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="映射", meta=(DisplayName="自定义表情映射", ToolTip="键为 ARKit 标准名称，值为模型形态键名称；优先于自动识别。")) TMap<FName,FName> MorphOverrides;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="逐形态键微调", meta=(DisplayName="形态键微调", ToolTip="键填写模型中的真实形态键名称。对最终输出应用倍率、偏移、上下限；个人配置中的同名微调优先。留空使用本视频默认方案。")) TMap<FName,FVFNMorphTweak> MorphTweaks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="骨骼", meta=(DisplayName="头部骨骼（空则自动）")) FName HeadBone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="骨骼", meta=(DisplayName="胸部骨骼（空则自动）")) FName ChestBone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="骨骼", meta=(DisplayName="左眼骨骼（空则自动）")) FName LeftEyeBone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="骨骼", meta=(DisplayName="右眼骨骼（空则自动）")) FName RightEyeBone;
};

USTRUCT(BlueprintType)
struct VRMFACENODES_API FVFNFrame
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕") TMap<FName,float> Curves;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕") TMap<FName,float> SourceCurves;
    UPROPERTY(BlueprintReadOnly, Category="面捕") TMap<FName,float> ManualCurves;
    UPROPERTY(BlueprintReadOnly, Category="面捕") TArray<FName> ManualMappedChannels;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕") FRotator Head = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕") bool bValid = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕") bool bHasHead = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕") bool bMetaHuman = false;
    UPROPERTY(BlueprintReadOnly, Category="面捕") double SourceTimestamp = 0.;
    UPROPERTY(BlueprintReadOnly, Category="面捕") TArray<FName> PerformerCalibratedChannels;
    // Auxiliary source data; never sent directly to a VRM morph or added to old mappings.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕") bool bHasJawOpenExtreme = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="面捕") float JawOpenExtreme = 0.f;
};

USTRUCT(BlueprintType)
struct VRMFACENODES_API FVFNState
{
    GENERATED_BODY()
    UPROPERTY(Transient, BlueprintReadOnly, Category="状态") float MotionProtection = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="状态") FRotator SmoothedHead = FRotator::ZeroRotator;
    UPROPERTY(Transient, BlueprintReadOnly, Category="状态") FRotator SmoothedBody = FRotator::ZeroRotator;
    UPROPERTY(Transient) TMap<FName,float> BlinkHistory;
    UPROPERTY(Transient) TMap<FName,float> FilterHistory;
    UPROPERTY(Transient) TMap<FName,float> MotionInputPrevious;
    UPROPERTY(Transient) TMap<FName,float> MotionInputOlder;
    UPROPERTY(Transient) bool bMotionFiltering = false;
    UPROPERTY(Transient) FRotator PreviousHead = FRotator::ZeroRotator;
    UPROPERTY(Transient) float TeethWeight = 0.f;
    UPROPERTY(Transient) bool bHeadInitialized = false;
    UPROPERTY(Transient) bool bFollowInitialized = false;
};
