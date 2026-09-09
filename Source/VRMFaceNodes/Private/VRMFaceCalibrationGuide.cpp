#include "VRMFaceCalibrationGuide.h"
#include "VRMFaceLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "Widgets/SWidget.h"

namespace
{
    FString CalibrationChannelLabel(FName Name)
    {
        FString N=Name.ToString(),Side;
        if(N.EndsWith(TEXT("Left"))){N.LeftChopInline(4);Side=TEXT("左侧");}
        else if(N.EndsWith(TEXT("Right"))){N.LeftChopInline(5);Side=TEXT("右侧");}
        static const TMap<FString,FString> Names={
            {TEXT("EyeBlink"),TEXT("闭眼")},{TEXT("EyeWide"),TEXT("睁大眼")},{TEXT("BrowInnerUp"),TEXT("眉毛内侧上抬")},
            {TEXT("BrowOuterUp"),TEXT("眉毛外侧上抬")},{TEXT("BrowDown"),TEXT("皱眉")},{TEXT("JawOpen"),TEXT("张嘴")},
            {TEXT("MouthLowerDown"),TEXT("下唇下拉")},{TEXT("MouthSmile"),TEXT("微笑")},{TEXT("CheekSquint"),TEXT("笑肌")},
            {TEXT("MouthFrown"),TEXT("嘴角下压")},{TEXT("MouthPucker"),TEXT("嘬嘴")},{TEXT("MouthFunnel"),TEXT("拢嘴")},
            {TEXT("MouthRollUpper"),TEXT("上唇内卷")},{TEXT("MouthRollLower"),TEXT("下唇内卷")},{TEXT("MouthPress"),TEXT("抿嘴")},
            {TEXT("MouthClose"),TEXT("闭嘴")},{TEXT("Mouth"),TEXT("歪嘴")},{TEXT("Jaw"),TEXT("下颌侧移")},
            {TEXT("MouthUpperUp"),TEXT("上唇上抬")},{TEXT("NoseSneer"),TEXT("皱鼻")},{TEXT("CheekPuff"),TEXT("鼓腮")},
            {TEXT("EyeLookUp"),TEXT("向上看")},{TEXT("EyeLookDown"),TEXT("向下看")},{TEXT("EyeLookIn"),TEXT("向内看")},{TEXT("EyeLookOut"),TEXT("向外看")}};
        const auto* Found=Names.Find(N);return Side+(Found?*Found:N);
    }
}

AVRMFaceCalibrationGuide::AVRMFaceCalibrationGuide() {PrimaryActorTick.bCanEverTick=true;BuildSteps();}
void AVRMFaceCalibrationGuide::BuildSteps()
{
    auto Add=[&](const TCHAR* Title,const TCHAR* Text,std::initializer_list<const TCHAR*> Names,float Seconds=5.f)
    {FVFNCalibrationStep S;S.Title=Title;S.Instruction=Text;S.Seconds=Seconds;for(const auto* N:Names) S.Channels.Add(FName(N));Steps.Add(S);};
    Add(TEXT("自然表情"),TEXT("正对镜头，双眼自然睁开，嘴唇轻轻合上。放松，不微笑、不说话，保持大致朝向即可，可以正常眨眼。"),{},5);
    Add(TEXT("嘴部"),TEXT("先按日常说话幅度张嘴，再按提示张到舒适最大幅度。两档分开采集。"),{TEXT("JawOpen")});
    Add(TEXT("双眼闭合"),TEXT("两只眼睛一起自然闭紧，保持约两秒，再睁开。重复一次，不要挤皱整张脸。"),{TEXT("EyeBlinkLeft"),TEXT("EyeBlinkRight")});
    Add(TEXT("双眼睁大"),TEXT("双眼一起睁到舒适最大幅度，尽量保持眉毛放松。保持两秒后放松。"),{TEXT("EyeWideLeft"),TEXT("EyeWideRight")});
    Add(TEXT("双侧抬眉"),TEXT("两边眉毛一起抬高，保持两秒，再回到自然表情。"),{TEXT("BrowInnerUp"),TEXT("BrowOuterUpLeft"),TEXT("BrowOuterUpRight")});
    Add(TEXT("双侧皱眉"),TEXT("两边眉毛一起向内下方靠拢，保持两秒后放松。"),{TEXT("BrowDownLeft"),TEXT("BrowDownRight")});
    Add(TEXT("微笑"),TEXT("左右嘴角一起上扬到舒适最大幅度，保持两秒，再放松。"),{TEXT("MouthSmileLeft"),TEXT("MouthSmileRight"),TEXT("CheekSquintLeft"),TEXT("CheekSquintRight")});
    Add(TEXT("嘴角下压"),TEXT("两侧嘴角一起向下，保持两秒，再放松。做不到的动作可以跳过。"),{TEXT("MouthFrownLeft"),TEXT("MouthFrownRight")});
    Add(TEXT("嘬嘴"),TEXT("像轻轻亲吻一样向前嘬嘴，达到舒适最大幅度；保持两秒，再放松。"),{TEXT("MouthPucker"),TEXT("MouthFunnel")});
    Add(TEXT("抿嘴"),TEXT("上下嘴唇轻轻向内卷并抿住，保持两秒，再放松。不要用牙齿咬住嘴唇。"),{TEXT("MouthRollUpper"),TEXT("MouthRollLower"),TEXT("MouthPressLeft"),TEXT("MouthPressRight"),TEXT("MouthClose")});
    Add(TEXT("左右歪嘴"),TEXT("嘴巴先向左到舒适最大幅度，回正，再向右。两个方向轮流做，头部尽量稳定。"),{TEXT("MouthLeft"),TEXT("MouthRight"),TEXT("JawLeft"),TEXT("JawRight")},8);
    Add(TEXT("双侧露上齿"),TEXT("两侧上唇一起轻轻抬起，露出上排牙齿，再放松。用于去除自然表情时的上唇残余抬起。"),{TEXT("MouthUpperUpLeft"),TEXT("MouthUpperUpRight"),TEXT("NoseSneerLeft"),TEXT("NoseSneerRight")});
    Add(TEXT("鼓腮"),TEXT("双侧脸颊一起鼓起，保持两秒后放松。"),{TEXT("CheekPuff")});
    Add(TEXT("眼球向上"),TEXT("头不动，两只眼睛一起向上看，到舒适范围即可。保持两秒，再看回镜头。"),{TEXT("EyeLookUpLeft"),TEXT("EyeLookUpRight")});
    Add(TEXT("眼球向下"),TEXT("头不动，两只眼睛一起向下看，保持两秒，再看回镜头。"),{TEXT("EyeLookDownLeft"),TEXT("EyeLookDownRight")});
    Add(TEXT("眼球左右"),TEXT("头不动，先一起向左看，回正，再一起向右看。左右方向轮流保持两秒。"),{TEXT("EyeLookInLeft"),TEXT("EyeLookOutLeft"),TEXT("EyeLookInRight"),TEXT("EyeLookOutRight")},8);
    Add(TEXT("轻微摇头"),TEXT("回到自然表情，嘴唇轻合、双眼睁开。缓慢左右转头，再轻轻点头，各约十度。不要快速甩头。"),{},7);
    for(int32 I:{2,3,5,6,7,9,10,11,13,14}) Steps[I].RequiredChannels=2;
    Steps[15].RequiredChannels=4;
}
UVFNCalibrationSubsystem* AVRMFaceCalibrationGuide::Service() const {return GetGameInstance()?GetGameInstance()->GetSubsystem<UVFNCalibrationSubsystem>():nullptr;}
void AVRMFaceCalibrationGuide::BeginPlay()
{
    Super::BeginPlay();StoredSlot=Service()?Service()->GetActiveSlot(ProfileSlot):ProfileSlot;SelectedProfile=StoredSlot;SessionSlot=ProfileSlot;
    Status=TEXT("准备好后开始。只记录面部曲线，不录制视频或声音。");
    if(auto* S=Service()) if(const auto* P=S->FindProfile(ProfileSlot)) {Candidate=*P;Status=TEXT("已自动读取保存的个人配置；可以重新校准。");}
    UVFNCalibrationLibrary::UpgradeProfile(Candidate);
    RefreshProfiles();RefreshMorphs();
    if(bComparisonLabels) BuildInterface();
}
void AVRMFaceCalibrationGuide::EndPlay(const EEndPlayReason::Type Why)
{
    if(bAutoSavePending&&!bRunning)SaveCalibration();
    CloseConfigurationWindow(true);
    if(auto* S=Service()) S->ClearPreview(SessionSlot);
    if(ToneAudio) ToneAudio->Stop();
    if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ToneTimer);
    if(Interface.IsValid() && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Interface.ToSharedRef());
    Interface.Reset();Super::EndPlay(Why);
}
void AVRMFaceCalibrationGuide::Signal()
{
    if(!bPromptSound) return;
    Tone=NewObject<USoundWaveProcedural>(this);Tone->SetSampleRate(24000);Tone->NumChannels=1;Tone->Duration=.18f;Tone->bLooping=false;Tone->bProcedural=true;
    TArray<int16> Samples;Samples.SetNumZeroed(4320);
    for(int32 I=0;I<Samples.Num();++I)
    {const float T=float(I)/24000.f;const float Envelope=FMath::Min(T/.015f,1.f)*FMath::Min((.18f-T)/.045f,1.f);Samples[I]=int16(FMath::Sin(2.f*PI*(T<.09f?660.f:880.f)*T)*Envelope*8000.f);}
    Tone->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));
    if(ToneAudio) ToneAudio->Stop();
    ToneAudio=UGameplayStatics::SpawnSound2D(this,Tone,.25f);
    if(GetWorld()) GetWorld()->GetTimerManager().SetTimer(ToneTimer,FTimerDelegate::CreateWeakLambda(this,[this](){if(ToneAudio)ToneAudio->Stop();}),.22f,false);
}
void AVRMFaceCalibrationGuide::StartCalibration()
{
    if(bRunning) {Status=TEXT("当前正在采集，请完成或取消后再开始。");return;}
    if(!UVFNCalibrationLibrary::IsValidSlot(ProfileSlot)) {Status=TEXT("先填写合法配置名称。");return;}
    if(auto* S=Service()) S->ClearPreview(ProfileSlot);
    SessionBefore=Candidate;bSessionWasPreviewing=bPreviewing;bSingleStep=false;bQuickCalibration=false;SingleTarget=-1;
    bAutoApplySession=false;
    // Recalibration replaces successful channels only. Missing or weak channels
    // retain the previous range, even during the full wizard.
    SessionSlot=ProfileSlot;SessionSubject=Subject;Candidate.Name=StoredSlot.IsNone()?ProfileSlot.ToString():StoredSlot.ToString();Candidate.Subject=Subject;
    NeutralSamples.Empty();NeutralHeads.Empty();StepResults.Empty();CalibrationDetails.Empty();bPreviewing=false;bComplete=false;
    if(!bPanelOpen) TogglePanel();BeginStep(0);
}
void AVRMFaceCalibrationGuide::StartQuickCalibration()
{
    if(bRunning) {Status=TEXT("当前正在采集，请完成或取消后再开始。");return;}
    StartCalibration();
    if(bRunning)bQuickCalibration=true;
}
TArray<FName> AVRMFaceCalibrationGuide::GetActiveChannels() const
{
    if(StepIndex==1)return {FName(TEXT("JawOpen"))};
    return Steps.IsValidIndex(StepIndex)?Steps[StepIndex].Channels:TArray<FName>();
}
void AVRMFaceCalibrationGuide::KeepEssentialRanges()
{
    static const TSet<FName> Essential={TEXT("JawOpen"),TEXT("EyeBlinkLeft"),TEXT("EyeBlinkRight"),TEXT("BrowInnerUp"),TEXT("BrowOuterUpLeft"),TEXT("BrowOuterUpRight")};
    // Cheek puff is often a weak input. Preserve an explicitly measured range
    // without adding another mandatory step to the quick wizard.
    for(auto It=Candidate.Ranges.CreateIterator();It;++It)if(!Essential.Contains(It.Key()) && It.Key()!=TEXT("CheekPuff"))It.RemoveCurrent();
}
void AVRMFaceCalibrationGuide::PreviewEssentialCalibration()
{
    if(bRunning){Status=TEXT("请先完成或取消当前采集。");return;}
    if(Candidate.Ranges.IsEmpty()){Status=TEXT("当前没有个人校准范围。可直接使用默认效果，或开始四步快速校准。");return;}
    if(!(bQuickCalibration && bComplete))
    {
        SessionBefore=Candidate;bSessionWasPreviewing=bPreviewing;SessionSlot=ProfileSlot;SessionSubject=Subject;
        bSingleStep=false;bQuickCalibration=true;StepIndex=Steps.Num();bComplete=true;Progress=1.f;
        KeepEssentialRanges();StepResults={TEXT("保留闭眼、嘴巴开合和抬眉的个人范围，以及已录制的鼓腮校准；其余使用已有自动映射。")};
        UpdateCalibrationDetails();
    }
    PreviewCandidate();Status=TEXT("正在预览精简效果，尚未保存。原配置可用「还原本次修改」恢复。");
}
void AVRMFaceCalibrationGuide::BeginStep(int32 Index)
{
    StepIndex=Index;Elapsed=0;FeedbackElapsed=0;PrepareSeconds=2.f;Progress=0.f;ActionSamples.Empty();MotionSpeeds.Empty();bHasMotionHead=false;
    if(Index==0){JawNeutralSamples.Empty();JawTypicalSamples.Empty();JawPhase=0;bJawInputChosen=false;bJawUseExtreme=false;bJawInputChanged=false;}
    if(Index==1){JawTypicalSamples.Empty();JawPhase=0;}
    if(!Steps.IsValidIndex(Index))
    {
        bRunning=false;bComplete=true;Progress=1.f;
        if(bQuickCalibration){KeepEssentialRanges();StepResults.Add(TEXT("已有鼓腮校准继续保留；其余表情回到已有自动映射。镜像、运动参数和模型微调保留。"));}
        FString M;UVFNCalibrationLibrary::ValidateProfile(Candidate,M);
        Status=(bQuickCalibration?TEXT("四组校准完成。请先预览，满意后保存。\n"):TEXT("采集完成。已采到的通道已更新，未采到的保留原设置；请先预览，再保存。\n"))+M;
        UpdateCalibrationDetails();Signal();
        if(bAutoApplySession)
        {
            UVFNCalibrationLibrary::UpgradeProfile(Candidate);
            const bool OK=SaveCalibration();
            if(OK)Status=TEXT("五步完成，已应用并保存。未采到的动作沿用原设置。");
            bAutoApplySession=false;
        }
        return;
    }
    bRunning=true;Status=TEXT("请先准备动作，提示音后开始采集。");UpdateCalibrationDetails();Signal();
}
void AVRMFaceCalibrationGuide::RetryStep()
{
    if(StepIndex==0) {NeutralSamples.Empty();NeutralHeads.Empty();}
    if(Steps.IsValidIndex(StepIndex)) BeginStep(StepIndex);
}
void AVRMFaceCalibrationGuide::SkipStep()
{
    if(StepIndex<=0) {Status=TEXT("自然表情不能跳过，它用于闭嘴基线和噪声估计。");return;}
    if(Steps.IsValidIndex(StepIndex)){StepResults.Add(Steps[StepIndex].Title+TEXT("：已保留原设置（跳过）。"));AdvanceStep();}
}
bool AVRMFaceCalibrationGuide::FinishStep()
{
    if(StepIndex==0)
    {
        int32 Count=0;for(const auto& Pair:NeutralSamples) Count=FMath::Max(Count,Pair.Value.Num());
        if(Count<20) {Status=TEXT("没有足够的新鲜面捕帧，请检查主题后重做。");return false;}
        if(bSingleStep && !SessionBefore.Ranges.IsEmpty() && SessionBefore.bMetaHuman!=CurrentFrame.bMetaHuman)
        {Status=TEXT("当前输入类型与原配置不同，请新建配置后校准，避免混用。");return false;}
        if(!bSingleStep && Candidate.bMetaHuman!=CurrentFrame.bMetaHuman)
        {Candidate.Ranges.Empty();Candidate.bRecenterHead=false;Candidate.bCameraCentered=false;Candidate.NeutralGaze.Empty();Candidate.bMotionMeasured=false;Candidate.MotionStart=90.f;Candidate.MotionFull=250.f;}
        Candidate.NeutralSamples=Count;Candidate.bMetaHuman=CurrentFrame.bMetaHuman;
        FString NeutralNote=TEXT("自然表情：已采集，正常眨眼和短暂波动由稳健基线处理。");
        if(NeutralHeads.Num()>=20)
        {
            TArray<float> Pitch,Yaw,Roll;const FRotator Ref=NeutralHeads[0];
            for(const auto& H:NeutralHeads) {const auto D=(H-Ref).GetNormalized();Pitch.Add(D.Pitch);Yaw.Add(D.Yaw);Roll.Add(D.Roll);}
            const bool StableHead=FMath::Max3(UVFNCalibrationLibrary::Percentile(Pitch,.9f)-UVFNCalibrationLibrary::Percentile(Pitch,.1f),UVFNCalibrationLibrary::Percentile(Yaw,.9f)-UVFNCalibrationLibrary::Percentile(Yaw,.1f),UVFNCalibrationLibrary::Percentile(Roll,.9f)-UVFNCalibrationLibrary::Percentile(Roll,.1f))<=12.f;
            if(StableHead)
            {Candidate.NeutralHead=(Ref+FRotator(UVFNCalibrationLibrary::Percentile(Pitch,.5f),UVFNCalibrationLibrary::Percentile(Yaw,.5f),UVFNCalibrationLibrary::Percentile(Roll,.5f))).GetNormalized();Candidate.bRecenterHead=true;}
            else NeutralNote+=TEXT("头部活动较多，保留原头部归零；面部校准继续。");
        }
        if(((bSingleStep && SingleTarget!=Steps.Num()-1) || bQuickCalibration) && !SessionBefore.Ranges.IsEmpty() && SessionBefore.bMetaHuman==CurrentFrame.bMetaHuman)
        {Candidate.NeutralHead=SessionBefore.NeutralHead;Candidate.bRecenterHead=SessionBefore.bRecenterHead;}
        StepResults.Add(NeutralNote);return true;
    }
    if(StepIndex==Steps.Num()-1)
    {
        if(MotionSpeeds.Num()<20 || UVFNCalibrationLibrary::Percentile(MotionSpeeds,.9f)<5.f) {Status=TEXT("尚未检测到足够头部运动；请轻轻转头、点头后重做，或跳过。");return false;}
        Candidate.MotionStart=FMath::Clamp(UVFNCalibrationLibrary::Percentile(MotionSpeeds,.75f)*.7f,35.f,120.f);
        Candidate.MotionFull=FMath::Clamp(FMath::Max(Candidate.MotionStart+60.f,UVFNCalibrationLibrary::Percentile(MotionSpeeds,.95f)*1.5f),95.f,320.f);
        Candidate.bMotionMeasured=true;StepResults.Add(TEXT("轻微摇头：已更新运动阈值。"));return true;
    }
    if(StepIndex==1)
    {
        const auto* Action=ActionSamples.Find(TEXT("JawOpen"));
        if(!bJawInputChosen || bJawInputChanged || !Action || Action->Num()<20)
        {Status=TEXT("嘴部输入缺失或采集中曲线类型改变，原设置已保留。请恢复同一采集源后重新开始嘴部校准。");return false;}
        if(JawPhase==0){JawTypicalSamples=*Action;return true;}
        FVFNCurveRange Fitted;FString M;
        if(!UVFNCalibrationLibrary::FitJawRange(JawNeutralSamples,JawTypicalSamples,*Action,bJawUseExtreme,Fitted,M)){Status=M;UpdateCalibrationDetails();return false;}
        Candidate.Ranges.Add(TEXT("JawOpen"),Fitted);Candidate.SchemaVersion=4;
        StepResults.Add(M);return true;
    }
    int32 Good=0;TArray<FString> Missing;TMap<FName,FVFNCurveRange> Fitted;
    for(FName N:GetActiveChannels())
    {
        const auto* Idle=NeutralSamples.Find(N);const auto* Action=ActionSamples.Find(N);FVFNCurveRange R;
        if(Idle && Action && UVFNCalibrationLibrary::FitRange(*Idle,*Action,R)) {Fitted.Add(N,R);++Good;}
        else Missing.Add(CalibrationChannelLabel(N));
    }
    UpdateCalibrationDetails();
    if(!Good) {Status=TEXT("本步未测到可靠变化，原设置已保留。可重做，或点「保留原设置并继续」；无需更加用力。");return false;}
    Candidate.SchemaVersion=FMath::Max(Candidate.SchemaVersion,3);
    for(const auto& Pair:Fitted) Candidate.Ranges.Add(Pair.Key,Pair.Value);
    StepResults.Add(FString::Printf(TEXT("%s：已更新 %d 项。"),*Steps[StepIndex].Title,Good)+(Missing.Num()?TEXT("保留原设置：")+FString::Join(Missing,TEXT("、"))+TEXT("。"):TEXT("")));
    return true;
}
void AVRMFaceCalibrationGuide::UpdateCalibrationDetails()
{
    if(!Steps.IsValidIndex(StepIndex)){CalibrationDetails=FString::Join(StepResults,TEXT("\n"));return;}
    if(StepIndex==0){CalibrationDetails=TEXT("自然基线采集中：不要求权重接近零，可以正常眨眼。\n左右通道独立记录，不要求两侧幅度相等。");return;}
    if(StepIndex==1)
    {
        const auto* Action=ActionSamples.Find(TEXT("JawOpen"));
        CalibrationDetails=FString::Printf(TEXT("嘴部第%d档：%s\n输入方式：%s\n闭嘴参考 %.3f · 普通张嘴 %.3f · 当前输入 %.3f\n只有两档都测到可靠差异才更新嘴部，未完成时原设置保留。"),JawPhase+1,
            JawPhase==0?TEXT("普通张嘴，目标约40%"):TEXT("最大张嘴，目标100%"),bJawUseExtreme?TEXT("MetaHuman 张嘴＋极限张嘴"):TEXT("单条张嘴曲线"),
            UVFNCalibrationLibrary::Percentile(JawNeutralSamples,.5f),UVFNCalibrationLibrary::Percentile(JawTypicalSamples,.8f),UVFNCalibrationLibrary::JawInput(CurrentFrame,bJawUseExtreme));
        if(Action)CalibrationDetails+=FString::Printf(TEXT("\n本档已采集 %d 帧。"),Action->Num());return;
    }
    if(StepIndex==Steps.Num()-1){CalibrationDetails=TEXT("轻微转头、点头即可；此步骤不要求改变表情。");return;}
    TArray<FString> Lines;
    if(StepResults.Num())Lines.Add(TEXT("上一项：")+StepResults.Last());
    for(FName N:GetActiveChannels())
    {
        const auto* Idle=NeutralSamples.Find(N);const auto* Action=ActionSamples.Find(N);const auto* Current=CurrentFrame.Curves.Find(N);
        if(!Idle || (!Current && !Action)){Lines.Add(CalibrationChannelLabel(N)+TEXT("：采集源未提供此通道，保留原设置。"));continue;}
        FVFNCurveRange Range;const bool Good=Action && UVFNCalibrationLibrary::FitRange(*Idle,*Action,Range);
        const bool Enough=Action && Action->Num()>=20;
        Lines.Add(FString::Printf(TEXT("%s：%s（当前 %.3f / 自然 %.3f / 峰值 %.3f）"),*CalibrationChannelLabel(N),
            Good?TEXT("已检测到有效变化"):Enough?TEXT("尚无明显变化，保留原设置"):TEXT("正在采集"),
            Current?*Current:0.f,Enough?Range.Neutral:UVFNCalibrationLibrary::Percentile(*Idle,.5f),Action?UVFNCalibrationLibrary::Percentile(*Action,.95f):0.f));
    }
    CalibrationDetails=FString::Join(Lines,TEXT("\n"));
}
void AVRMFaceCalibrationGuide::Tick(float Dt)
{
    Super::Tick(Dt);
    if(bAutomaticInput) FeedFrame(UVRMFaceLibrary::ReadLiveLink(Subject),Dt);
    if(bAutoSavePending&&!bRunning&&!bCenteringCamera)
    {
        AutoSaveDelay-=Dt;
        if(AutoSaveDelay<=0.f){bAutoSavePending=false;SaveCalibration();}
    }
}
void AVRMFaceCalibrationGuide::FeedFrame(const FVFNFrame& Frame,float Dt)
{
    if(!FMath::IsFinite(Dt) || Dt<=0.f) return;
    CurrentFrame=Frame;
    const bool NewFrame=CurrentFrame.bValid && CurrentFrame.SourceTimestamp>0. && CurrentFrame.SourceTimestamp!=LastTimestamp;
    StaleSeconds=NewFrame?0.f:StaleSeconds+FMath::Max(Dt,0.f);bFreshInput=CurrentFrame.bValid && StaleSeconds<.3f;
    PendingSampleSeconds=StaleSeconds>.3f?0.f:PendingSampleSeconds+FMath::Max(Dt,0.f);
    if(StaleSeconds>.3f) bHasMotionHead=false;
    if(NewFrame) LastTimestamp=CurrentFrame.SourceTimestamp;
    FeedCamera(Frame,Dt,NewFrame);
    if(!bRunning) return;
    if(bCalibrationWindowPaused) {PendingSampleSeconds=0.f;bHasMotionHead=false;return;}
    if(ProfileSlot!=SessionSlot || Subject!=SessionSubject) {bRunning=false;Status=TEXT("主题或配置名已改变，请重新开始，避免混合不同输入。");return;}
    if(!NewFrame) {if(!bFreshInput) Status=TEXT("等待新的面捕数据，计时已暂停。请恢复采集或回放。");return;}
    if(StepIndex>0 && Candidate.bMetaHuman!=CurrentFrame.bMetaHuman) {bRunning=false;Status=TEXT("输入类型发生变化，请重新开始校准。");return;}
    const float SampleDt=FMath::Clamp(PendingSampleSeconds,0.f,.1f);PendingSampleSeconds=0.f;
    if(PrepareSeconds>0)
    {PrepareSeconds=FMath::Max(0.f,PrepareSeconds-SampleDt);Status=FString::Printf(TEXT("准备 %.0f 秒…"),FMath::CeilToFloat(PrepareSeconds));if(PrepareSeconds<=0) Signal();return;}
    Elapsed+=SampleDt;Progress=FMath::Clamp(Elapsed/Steps[StepIndex].Seconds,0.f,1.f);if(StepIndex==1)Progress=.5f*(JawPhase+Progress);
    if(StepIndex==0)
    {
        for(const auto& P:CurrentFrame.Curves) if(FMath::IsFinite(P.Value)) NeutralSamples.FindOrAdd(P.Key).Add(P.Value);if(CurrentFrame.bHasHead) NeutralHeads.Add(CurrentFrame.Head);
        if(const auto* J=CurrentFrame.Curves.Find(TEXT("JawOpen")))if(FMath::IsFinite(*J))
        {
            const bool Extended=CurrentFrame.bMetaHuman && CurrentFrame.bHasJawOpenExtreme;
            if(!bJawInputChosen){bJawUseExtreme=Extended;bJawInputChosen=true;}
            if(Extended!=bJawUseExtreme)bJawInputChanged=true;
            JawNeutralSamples.Add(UVFNCalibrationLibrary::JawInput(CurrentFrame,bJawUseExtreme));
        }
    }
    else if(StepIndex==Steps.Num()-1)
    {if(CurrentFrame.bHasHead && bHasMotionHead && SampleDt>.001f) {const auto D=(CurrentFrame.Head-PreviousMotionHead).GetNormalized();MotionSpeeds.Add(FVector(D.Pitch,D.Yaw,D.Roll).Size()/SampleDt);}PreviousMotionHead=CurrentFrame.Head;bHasMotionHead=CurrentFrame.bHasHead;}
    else for(FName N:GetActiveChannels()) if(const float* V=CurrentFrame.Curves.Find(N)) if(FMath::IsFinite(*V))
    {
        if(StepIndex==1)
        {
            if((CurrentFrame.bMetaHuman && CurrentFrame.bHasJawOpenExtreme)!=bJawUseExtreme)bJawInputChanged=true;
            ActionSamples.FindOrAdd(N).Add(UVFNCalibrationLibrary::JawInput(CurrentFrame,bJawUseExtreme));
        }
        else ActionSamples.FindOrAdd(N).Add(*V);
    }
    Status=FString::Printf(TEXT("采集中：%.1f / %.0f 秒。"),Elapsed,Steps[StepIndex].Seconds);
    FeedbackElapsed+=SampleDt;if(FeedbackElapsed>=.25f){FeedbackElapsed=0.f;UpdateCalibrationDetails();}
    if(Elapsed>=Steps[StepIndex].Seconds)
    {
        bRunning=false;
        if(FinishStep())
        {
            if(StepIndex==1 && JawPhase==0)
            {JawPhase=1;Elapsed=0;FeedbackElapsed=0;PrepareSeconds=2.f;Progress=.5f;ActionSamples.Empty();bRunning=true;Status=TEXT("普通张嘴已记录。提示音后张到舒适最大幅度。");UpdateCalibrationDetails();Signal();}
            else AdvanceStep();
        }
        else if(bAutoApplySession && StepIndex>0)
        {StepResults.Add(Steps[StepIndex].Title+TEXT("：未测到可靠变化，已沿用原设置。"));AdvanceStep();}
        else Signal();
    }
}
FString AVRMFaceCalibrationGuide::GetPrompt() const
{
    if(bAutoApplySession)
    {
        const int32 Position=StepIndex==0?1:StepIndex==2?2:StepIndex==1?3+JawPhase:5;
        const FString Title=StepIndex==1?(JawPhase==0?TEXT("普通张嘴"):TEXT("最大张嘴")):StepIndex==2?TEXT("闭眼"):StepIndex==4?TEXT("抬眉"):TEXT("自然表情");
        const FString Action=StepIndex==0?TEXT("自然坐正看屏幕，嘴唇轻合，可以正常眨眼。"):StepIndex==2?TEXT("双眼一起自然闭紧，保持两秒后睁开，再做一次。"):StepIndex==4?TEXT("两边眉毛一起抬高，保持两秒，再放松。"):JawPhase==0?TEXT("像平常聊天说“啊”，保持两秒后放松，再做一次。"):TEXT("张到舒适最大幅度，保持两秒后放松，再做一次。");
        return FString::Printf(TEXT("%d / 5 · %s\n%s"),Position,*Title,*Action);
    }
    if(!Steps.IsValidIndex(StepIndex))return bComplete?TEXT("校准完成。"):TEXT("自然表情 → 闭眼 → 普通张嘴 → 最大张嘴 → 抬眉\n五步约35秒，完成即使用。");
    const int32 Position=bQuickCalibration?QuickSteps.Find(StepIndex)+1:bSingleStep?(StepIndex==0?1:2):StepIndex+1;
    const int32 Total=bQuickCalibration?QuickSteps.Num():bSingleStep?2:Steps.Num();
    const FString Title=StepIndex==1?(JawPhase==0?TEXT("普通张嘴 · 嘴部1/2"):TEXT("最大张嘴 · 嘴部2/2")):Steps[StepIndex].Title;
    const FString Instruction=StepIndex==1?(JawPhase==0?TEXT("像平常聊天时说“啊”，不要刻意张大。保持约两秒，再放松，重复一次。"):TEXT("现在张到舒适的最大幅度，明显大于刚才说话。保持约两秒，再放松，重复一次。不要勉强用力。")):Steps[StepIndex].Instruction;
    return (bQuickCalibration?TEXT("快速校准 · "):bSingleStep?TEXT("单项校准 · "):TEXT("完整校准 · "))+FString::Printf(TEXT("%02d / %02d  %s\n%s"),Position,Total,*Title,*Instruction);
}
void AVRMFaceCalibrationGuide::TogglePreview()
{
    if(!Service()) return;
    if(bPreviewing) {Service()->ClearPreview(ProfileSlot);bPreviewing=false;Status=TEXT("已回到已保存配置或原参数。");return;}
    FString M;if(!UVFNCalibrationLibrary::ValidateProfile(Candidate,M)) {Status=M;return;}
    Service()->SetPreview(ProfileSlot,Candidate);bPreviewing=true;Status=TEXT("正在预览本次校准，尚未保存。\n")+M;
}
bool AVRMFaceCalibrationGuide::SaveCalibration()
{
    if(bRunning) {Status=TEXT("请完成或暂停当前动作后再保存。");return false;}
    if(!Service()) return false;if(StoredSlot.IsNone())StoredSlot=ProfileSlot;
    const bool OK=Service()->Save(StoredSlot,Candidate,Status);
    if(OK) {if(!Service()->Activate(ProfileSlot,StoredSlot,Status))return false;Candidate=*Service()->FindProfile(ProfileSlot);SessionBefore=Candidate;bSessionWasPreviewing=false;bQuickCalibration=false;bPreviewing=false;RefreshProfiles();Status=TEXT("已保存并应用：")+StoredSlot.ToString();}return OK;
}
bool AVRMFaceCalibrationGuide::LoadCalibration()
{
    if(bRunning) {Status=TEXT("请先取消当前采集，再读取保存配置。");return false;}
    return SelectProfile(StoredSlot.IsNone()?ProfileSlot:StoredSlot);
}
void AVRMFaceCalibrationGuide::CancelCalibration()
{
    bAutoApplySession=false;bAutoSavePending=false;
    bRunning=false;bComplete=false;
    if(StepIndex>=0)
    {
        Candidate=SessionBefore;bPreviewing=false;
        if(auto* S=Service())
        {
            if(bSessionWasPreviewing){S->SetPreview(SessionSlot,Candidate);bPreviewing=true;}
            else S->ClearPreview(SessionSlot);
        }
    }
    bSessionWasPreviewing=false;bQuickCalibration=false;StepIndex=-1;Progress=0.f;CalibrationDetails.Empty();StepResults.Empty();Status=TEXT("已还原本次修改，其他校准和保存配置保持不变。");
}

void AVRMFaceCalibrationGuide::StartSingleStep(int32 ActionIndex)
{
    if(ActionIndex<=0 || !Steps.IsValidIndex(ActionIndex) || bRunning) {Status=TEXT("请先停止当前采集，选择一项动作。");return;}
    if(!UVFNCalibrationLibrary::IsValidSlot(ProfileSlot)) {Status=TEXT("配置绑定名称不合法。");return;}
    SessionBefore=Candidate;bSessionWasPreviewing=bPreviewing;bSingleStep=true;bQuickCalibration=false;SingleTarget=ActionIndex;SessionSlot=ProfileSlot;SessionSubject=Subject;
    if(auto* S=Service())S->ClearPreview(ProfileSlot);
    Candidate.Name=StoredSlot.IsNone()?ProfileSlot.ToString():StoredSlot.ToString();Candidate.Subject=Subject;
    NeutralSamples.Empty();NeutralHeads.Empty();StepResults.Empty();CalibrationDetails.Empty();bPreviewing=false;bComplete=false;
    if(!bPanelOpen)TogglePanel();BeginStep(0);
}
void AVRMFaceCalibrationGuide::AdvanceStep()
{
    if(bQuickCalibration){const int32 Next=QuickSteps.Find(StepIndex)+1;BeginStep(QuickSteps.IsValidIndex(Next)?QuickSteps[Next]:Steps.Num());}
    else if(bSingleStep)BeginStep(StepIndex==0?SingleTarget:Steps.Num());else BeginStep(StepIndex+1);
}
void AVRMFaceCalibrationGuide::PreviewCandidate()
{
    FString M;if(!UVFNCalibrationLibrary::ValidateProfile(Candidate,M)){Status=M;return;}
    if(auto* S=Service()){S->SetPreview(ProfileSlot,Candidate);bPreviewing=true;Status=TEXT("正在预览，尚未保存。满意后点击「保存配置」。");}
}
bool AVRMFaceCalibrationGuide::SetMorphTweak(FName Morph,const FVFNMorphTweak& Tweak)
{
    if(bRunning || Morph.IsNone()){Status=TEXT("请结束当前采集后再微调。");return false;}
    auto Copy=Candidate;Copy.MorphTweaks.Add(Morph,Tweak);FString M;
    if(!UVFNCalibrationLibrary::ValidateProfile(Copy,M)){Status=M;return false;}
    Candidate=MoveTemp(Copy);if(TuningModel)Candidate.ModelPath=TuningModel->GetPathName();QueueAutoSave();return true;
}
void AVRMFaceCalibrationGuide::ResetMorphTweak(FName Morph) {if(!bRunning){Candidate.MorphTweaks.Remove(Morph);QueueAutoSave();}}
void AVRMFaceCalibrationGuide::SetPersonalJawWithManual(bool Enabled) {if(!bRunning){Candidate.bUsePersonalJawWithManual=Enabled;PreviewCandidate();Status=Enabled?TEXT("已预览个人嘴部校准；保存后下次沿用。"):TEXT("已预览内置手调口型；录制数据仍保留，保存后下次沿用。");}}
void AVRMFaceCalibrationGuide::SetMirrorCapture(bool Enabled) {if(!bRunning){Candidate.bOverrideMirror=true;Candidate.bMirrorCapture=Enabled;QueueAutoSave();}}
bool AVRMFaceCalibrationGuide::CreateProfile(const FString& Name)
{
    if(bRunning || !Service())return false;const FName Slot(*Name.TrimStartAndEnd());
    if(!UVFNCalibrationLibrary::IsValidSlot(Slot) || Service()->ListProfiles().Contains(Slot)){Status=TEXT("请填写尚未使用的新名称。");return false;}
    FVFNPerformerProfile Empty;Empty.Name=Slot.ToString();if(!Service()->Save(Slot,Empty,Status))return false;return SelectProfile(Slot);
}
bool AVRMFaceCalibrationGuide::SelectProfile(FName Name)
{
    if(bRunning || !Service())return false;
    if(!Service()->Activate(ProfileSlot,Name,Status))return false;
    StoredSlot=Name;SelectedProfile=Name;Candidate=*Service()->FindProfile(ProfileSlot);bPreviewing=false;bComplete=false;StepIndex=-1;bPendingDelete=false;RefreshProfiles();return true;
}
