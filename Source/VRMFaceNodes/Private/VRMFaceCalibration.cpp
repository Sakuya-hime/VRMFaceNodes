#include "VRMFaceCalibration.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
    FString DiskSlot(FName N) { return TEXT("VRMFaceNodes_")+N.ToString(); }
    UVFNCalibrationSubsystem* Service(const UObject* Context)
    {
        UWorld* World=GEngine?GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
        return World && World->GetGameInstance()?World->GetGameInstance()->GetSubsystem<UVFNCalibrationSubsystem>():nullptr;
    }
    bool ValidJawAnchors(const FVFNCurveRange& R)
    {
        if(!FMath::IsFinite(R.Typical) || !FMath::IsFinite(R.RestCeiling) || !FMath::IsFinite(R.Maximum))return false;
        const float A=R.Typical-R.RestCeiling,B=R.Maximum-R.Typical,Limit=R.bUseJawOpenExtreme?8.f:4.f;
        return A>0.f && B>0.f && .4f/A<=Limit+.001f && .6f/B<=Limit+.001f;
    }
    float JawResponse(float X,const FVFNCurveRange& R)
    {
        if(X<=R.RestCeiling)return 0.f;if(X>=R.Maximum)return 1.f;
        const float H0=R.Typical-R.RestCeiling,H1=R.Maximum-R.Typical;
        const float S0=.4f/H0,S1=.6f/H1;
        // Monotone cubic Hermite interpolation through 0, .4, 1. Shared tangent
        // at the middle anchor avoids a jump or a flat spot in normal speech.
        const float W0=2.f*H1+H0,W1=H1+2.f*H0;
        const float M1=(W0+W1)/(W0/S0+W1/S1);
        const float M0=FMath::Clamp(((2.f*H0+H1)*S0-H0*S1)/(H0+H1),0.f,3.f*S0);
        const float M2=FMath::Clamp(((2.f*H1+H0)*S1-H1*S0)/(H0+H1),0.f,3.f*S1);
        const bool Lower=X<R.Typical;
        const float H=Lower?H0:H1,T=FMath::Clamp((X-(Lower?R.RestCeiling:R.Typical))/H,0.f,1.f);
        const float T2=T*T,T3=T2*T,A=Lower?0.f:.4f,B=Lower?.4f:1.f;
        const float Y=(2*T3-3*T2+1)*A+(T3-2*T2+T)*H*(Lower?M0:M1)+(-2*T3+3*T2)*B+(T3-T2)*H*(Lower?M1:M2);
        return FMath::Clamp(Y,A,B);
    }
    float StableJawPeak(const TArray<float>& Values)
    {
        TArray<float> Filtered;
        for(int32 I=3;I+3<Values.Num();++I)
        {TArray<float> Window;for(int32 J=I-3;J<=I+3;++J)Window.Add(Values[J]);Filtered.Add(UVFNCalibrationLibrary::Percentile(Window,.5f));}
        return UVFNCalibrationLibrary::Percentile(Filtered,.8f);
    }
}
bool UVFNCalibrationLibrary::IsValidSlot(FName Slot)
{
    const FString S=Slot.ToString();
    return !Slot.IsNone() && Slot!=TEXT("ProfileCatalog") && S==S.TrimStartAndEnd() && !S.EndsWith(TEXT(".")) && S.Len()<=64 && !S.Contains(TEXT("..")) && !S.Contains(TEXT("/")) && !S.Contains(TEXT("\\")) && !S.Contains(TEXT(":")) && !S.Contains(TEXT("*")) && !S.Contains(TEXT("?")) && !S.Contains(TEXT("\"")) && !S.Contains(TEXT("<")) && !S.Contains(TEXT(">")) && !S.Contains(TEXT("|"));
}
float UVFNCalibrationLibrary::Percentile(TArray<float> Values, float Fraction)
{
    Values.RemoveAll([](float V){return !FMath::IsFinite(V);});
    if(Values.IsEmpty()) return 0.f;
    Values.Sort();
    const float P=FMath::Clamp(Fraction,0.f,1.f)*(Values.Num()-1);
    const int32 I=FMath::FloorToInt(P);
    return FMath::Lerp(Values[I],Values[FMath::Min(I+1,Values.Num()-1)],P-I);
}
bool UVFNCalibrationLibrary::FitRange(const TArray<float>& Neutral, const TArray<float>& Action, FVFNCurveRange& Out)
{
    Out={};
    TArray<float> Idle=Neutral,Active=Action;
    const auto Invalid=[](float V){return !FMath::IsFinite(V) || V<0.f || V>1.f;};
    Idle.RemoveAll(Invalid);Active.RemoveAll(Invalid);
    if(Idle.Num()<20 || Active.Num()<20) return false;
    // Natural blinks and brief facial movements are outliers, not a failed
    // neutral pose. Estimate noise from the central cluster of neutral samples.
    const float Median=Percentile(Idle,.5f);
    TArray<float> Deviations;for(float V:Idle)Deviations.Add(FMath::Abs(V-Median));
    const float Radius=FMath::Max(.015f,3.f*1.4826f*Percentile(Deviations,.5f));
    Idle.RemoveAll([&](float V){return FMath::Abs(V-Median)>Radius;});
    if(Idle.Num()<20)return false;
    Out.Neutral=Percentile(Idle,.5f);
    Out.RestCeiling=FMath::Min(Percentile(Idle,.9f)+.003f,1.f);
    Out.Maximum=Percentile(Active,.95f);Out.SampleCount=Active.Num();
    const float RequiredSpan=FMath::Max(.025f,2.f*(Percentile(Idle,.9f)-Percentile(Idle,.1f)));
    Out.MaxGain=FMath::Clamp(1.f/RequiredSpan,4.f,40.f);
    Out.bEnabled=Out.Maximum-Out.RestCeiling>=RequiredSpan;
    return Out.bEnabled;
}
float UVFNCalibrationLibrary::JawInput(const FVFNFrame& Frame,bool bUseExtreme)
{
    const float Base=Frame.Curves.FindRef(TEXT("JawOpen"));
    if(!FMath::IsFinite(Base))return 0.f;
    const float Extra=Frame.bHasJawOpenExtreme && FMath::IsFinite(Frame.JawOpenExtreme)?FMath::Clamp(Frame.JawOpenExtreme,0.f,1.f):0.f;
    return bUseExtreme?.5f*(FMath::Clamp(Base,0.f,1.f)+Extra):FMath::Clamp(Base,0.f,1.f);
}
bool UVFNCalibrationLibrary::FitJawRange(const TArray<float>& Neutral,const TArray<float>& Typical,const TArray<float>& Maximum,bool bUseExtreme,FVFNCurveRange& Out,FString& Message)
{
    Out={};TArray<float> Idle=Neutral,Normal=Typical,Wide=Maximum;
    const auto Invalid=[](float V){return !FMath::IsFinite(V)||V<0.f||V>1.f;};Idle.RemoveAll(Invalid);Normal.RemoveAll(Invalid);Wide.RemoveAll(Invalid);
    if(Idle.Num()<20||Normal.Num()<20||Wide.Num()<20){Message=TEXT("嘴部采样不足，原设置已保留。请恢复采集后重做嘴部。");return false;}
    FVFNCurveRange R;
    if(!FitRange(Idle,Wide,R)){Message=TEXT("没有测到可靠的张嘴变化，原设置已保留。请检查面捕输入，不必更加用力。");return false;}
    R.Typical=StableJawPeak(Normal);R.Maximum=StableJawPeak(Wide);R.bUseJawOpenExtreme=bUseExtreme;R.MaxGain=bUseExtreme?8.f:4.f;
    if(R.Typical<=R.RestCeiling){Message=TEXT("普通张嘴与闭嘴没有区分开，原设置已保留。请按日常说话幅度重做嘴部。");return false;}
    if(R.Maximum-R.Typical<FMath::Max(bUseExtreme?.015f:.025f,2.f*(R.RestCeiling-R.Neutral)))
    {Message=TEXT("普通张嘴与最大张嘴的输入太接近，原设置已保留。请检查回放是否包含两档动作；无需用力挤脸。");return false;}
    if(!ValidJawAnchors(R)){Message=TEXT("嘴部信号变化过小，放大会过于敏感，已保留原设置。请检查采集或重做两档张嘴。");return false;}
    R.bThreePointJaw=true;R.bEnabled=true;R.SampleCount=Normal.Num()+Wide.Num();Out=R;
    Message=bUseExtreme?TEXT("嘴部已记录三点，并使用 MetaHuman 极限张嘴辅助曲线。"):TEXT("嘴部已记录三点：普通张嘴约40%，最大张嘴100%。");return true;
}
bool UVFNCalibrationLibrary::ValidateProfile(const FVFNPerformerProfile& P,FString& Message)
{
    if((P.SchemaVersion<1 || P.SchemaVersion>5) || ((!P.Ranges.IsEmpty() || P.bRecenterHead || P.bMotionMeasured) && P.NeutralSamples<20) || P.Ranges.Num()>52 || P.MorphTweaks.Num()>2048 || P.NeutralHead.ContainsNaN()) {Message=TEXT("配置缺少有效自然表情，或版本不兼容。请重新校准。");return false;}
    if(!FMath::IsFinite(P.SpeechAmount)||P.SpeechAmount<.5f||P.SpeechAmount>1.5f||!FMath::IsFinite(P.ChinStrength)||P.ChinStrength<0.f||P.ChinStrength>1.f||P.NeutralGaze.Num()>4)
    {Message=TEXT("表情幅度或摄像头配置无效，保留原设置。");return false;}
    for(const auto& G:P.NeutralGaze)if(!FMath::IsFinite(G.Value)||FMath::Abs(G.Value)>1.f || !(G.Key==TEXT("HorizontalLeft")||G.Key==TEXT("HorizontalRight")||G.Key==TEXT("VerticalLeft")||G.Key==TEXT("VerticalRight")))
    {Message=TEXT("视线校正配置无效，保留原设置。");return false;}
    int32 Enabled=0;
    for(const auto& Pair:P.Ranges)
    {
        const auto& R=Pair.Value;
        if(!FMath::IsFinite(R.Neutral) || !FMath::IsFinite(R.RestCeiling) || !FMath::IsFinite(R.Maximum) || !FMath::IsFinite(R.MaxGain) || R.MaxGain<1.f || R.MaxGain>40.f || R.Neutral<0 || R.RestCeiling<R.Neutral || R.RestCeiling>1.f || R.Maximum>1.f || R.Maximum<0 || (R.bEnabled && R.Maximum-R.RestCeiling<(P.SchemaVersion>=3?.0249f:.099f))) {Message=TEXT("配置包含无效范围，未应用。");return false;}
        if(!FMath::IsFinite(R.Typical) || R.Typical<0.f || R.Typical>1.f || (R.bUseJawOpenExtreme && !R.bThreePointJaw) || (R.bThreePointJaw && (P.SchemaVersion<4 || Pair.Key!=TEXT("JawOpen") || !R.bEnabled || !ValidJawAnchors(R) || (R.bUseJawOpenExtreme && !P.bMetaHuman))))
        {Message=TEXT("嘴部三点范围无效，未应用。请使用本版单独校准嘴部。");return false;}
        if(R.bEnabled) ++Enabled;
    }
    for(const auto& Pair:P.MorphTweaks)
    {
        const auto& T=Pair.Value;
        if(Pair.Key.IsNone() || Pair.Key.ToString().Len()>256 || !FMath::IsFinite(T.Gain) || !FMath::IsFinite(T.Offset) || !FMath::IsFinite(T.Minimum) || !FMath::IsFinite(T.Maximum) || T.Gain<0 || T.Gain>3 || T.Offset< -1 || T.Offset>1 || T.Minimum<0 || T.Maximum>1.5f || T.Maximum<T.Minimum)
        {Message=TEXT("形态键微调参数无效，未应用。请检查上下限和幅度。");return false;}
    }
    if(!FMath::IsFinite(P.MotionStart) || !FMath::IsFinite(P.MotionFull) || P.MotionStart<1.f || P.MotionFull<=P.MotionStart) {Message=TEXT("运动参数无效。");return false;}
    Message=FString::Printf(TEXT("%d 个校准通道，%d 项形态键微调；其余使用本视频默认方案。"),Enabled,P.MorphTweaks.Num());return true;
}
FVFNFrame UVFNCalibrationLibrary::ApplyProfile(const FVFNFrame& Frame,const FVFNPerformerProfile& P)
{
    FVFNFrame R=Frame;
    // A saved MetaHuman baseline cannot be silently applied to an ARKit producer.
    if(!Frame.bValid || P.bMetaHuman!=Frame.bMetaHuman) return R;
    for(auto& Pair:R.Curves)
    {
        if(Pair.Key==TEXT("JawOpen") && Frame.ManualCurves.Contains(Pair.Key) && !P.bUsePersonalJawWithManual)continue;
        const auto* Range=P.Ranges.Find(Pair.Key);
        if(!Range || !Range->bEnabled || !FMath::IsFinite(Pair.Value)) continue;
        if(Pair.Key==TEXT("CheekPuff"))if(const float* Manual=Frame.ManualCurves.Find(Pair.Key))Pair.Value=*Manual;
        if(P.SchemaVersion>=4 && Pair.Key==TEXT("JawOpen") && Range->bThreePointJaw)
        {
            if(ValidJawAnchors(*Range)){Pair.Value=JawResponse(JawInput(Frame,Range->bUseJawOpenExtreme),*Range);R.PerformerCalibratedChannels.AddUnique(Pair.Key);}
            continue;
        }
        const float Span=FMath::Max(Range->Maximum-Range->RestCeiling,P.SchemaVersion>=3?.025f:.1f);
        // Version 1/2 profiles keep their original fourfold response. Newly
        // measured weak-but-clean channels can reach the performer's own maximum.
        const float GainLimit=P.SchemaVersion>=3?FMath::Clamp(Range->MaxGain,1.f,40.f):4.f;
        Pair.Value=FMath::Clamp(FMath::Max(0.f,Pair.Value-Range->RestCeiling)*FMath::Min(1.f/Span,GainLimit),0.f,1.f);
        R.PerformerCalibratedChannels.AddUnique(Pair.Key);
    }
    if(P.bRecenterHead && R.bHasHead) R.Head=P.bCameraCentered?RelativeCameraHead(R.Head,P):(R.Head-P.NeutralHead).GetNormalized();
    if(P.bCameraCentered)for(const TCHAR* Side:{TEXT("Left"),TEXT("Right")})for(bool Horizontal:{true,false})
    {
        const FName Pos(FString(Horizontal?TEXT("EyeLookOut"):TEXT("EyeLookUp"))+Side),Neg(FString(Horizontal?TEXT("EyeLookIn"):TEXT("EyeLookDown"))+Side);
        const FName Key(FString(Horizontal?TEXT("Horizontal"):TEXT("Vertical"))+Side);
        if(!R.Curves.Contains(Pos)||!R.Curves.Contains(Neg)||!P.NeutralGaze.Contains(Key))continue;
        const float V=FMath::Clamp(R.Curves.FindRef(Pos)-R.Curves.FindRef(Neg)-P.NeutralGaze.FindRef(Key),-1.f,1.f);
        R.Curves.Add(Pos,FMath::Max(V,0.f));R.Curves.Add(Neg,FMath::Max(-V,0.f));
    }
    return R;
}
void UVFNCalibrationLibrary::UpgradeProfile(FVFNPerformerProfile& P)
{
    if(P.SchemaVersion>=5)return;
    // Preserve every recorded anchor and its input domain. The simplified mode
    // combines the hand-tuned lip mapping with the previous personal jaw range.
    P.bUsePersonalJawWithManual=true;P.SchemaVersion=5;
}
FRotator UVFNCalibrationLibrary::RelativeCameraHead(const FRotator& Head,const FVFNPerformerProfile& P)
{
    auto ToRotation=[&P](FRotator H){if(P.bCameraHeadRadians)H*=180.f/PI;return P.bCameraVRoidAxes?FRotator(-H.Pitch,H.Roll,H.Yaw):H;};
    const FQuat N=ToRotation(P.NeutralHead).Quaternion(),C=ToRotation(Head).Quaternion();
    const FRotator D=(N.Inverse()*C).GetNormalized().Rotator();
    FRotator R=P.bCameraVRoidAxes?FRotator(-D.Pitch,D.Roll,D.Yaw):D;
    if(P.bCameraHeadRadians)R*=PI/180.f;return R;
}
float UVFNCalibrationLibrary::SpeechResponse(float V,float Amount)
{
    if(!FMath::IsFinite(V))return 0.f;
    const float X=FMath::Clamp(V,0.f,1.f);
    // Monotone, with exact 0 and 1 endpoints: normal speech gains expression
    // without clipping early or lifting a closed mouth off its baseline.
    return X+(FMath::Clamp(Amount,.5f,1.5f)-1.f)*X*(1.f-X);
}
const FVFNPerformerProfile* UVFNCalibrationSubsystem::FindProfile(FName Slot)
{
    if(!UVFNCalibrationLibrary::IsValidSlot(Slot)) return nullptr;
    if(const auto* P=Previews.Find(Slot)) return P;
    Slot=GetActiveSlot(Slot);
    if(!Attempted.Contains(Slot)) {FString M;Reload(Slot,M);}
    return Profiles.Find(Slot);
}
void UVFNCalibrationSubsystem::SetPreview(FName Slot,const FVFNPerformerProfile& P)
{
    FString M;if(UVFNCalibrationLibrary::IsValidSlot(Slot) && UVFNCalibrationLibrary::ValidateProfile(P,M)) {Previews.Add(Slot,P);++Revision;}
}
void UVFNCalibrationSubsystem::ClearPreview(FName Slot) {if(Previews.Remove(Slot)) ++Revision;}
bool UVFNCalibrationSubsystem::Save(FName Slot,const FVFNPerformerProfile& P,FString& Message)
{
    if(!UVFNCalibrationLibrary::IsValidSlot(Slot)) {Message=TEXT("配置名称不合法：请使用 1～64 个文字、字母或数字，不含路径符号。");return false;}
    if(!UVFNCalibrationLibrary::ValidateProfile(P,Message)) return false;
    auto* Save=Cast<UVFNCalibrationSave>(UGameplayStatics::CreateSaveGameObject(UVFNCalibrationSave::StaticClass()));
    Save->Profile=P;Save->Profile.Name=Slot.ToString();Save->Profile.SavedAt=FDateTime::UtcNow().ToIso8601();
    if(!UGameplayStatics::SaveGameToSlot(Save,DiskSlot(Slot),0)) {Message=TEXT("写入失败，原配置未在内存中替换。请检查磁盘和目录权限。");return false;}
    Profiles.Add(Slot,Save->Profile);Previews.Remove(Slot);Attempted.Add(Slot);++Revision;
    Message=TEXT("已保存并应用：")+Slot.ToString()+TEXT("。下次运行会自动读取。");return true;
}
bool UVFNCalibrationSubsystem::Reload(FName Slot,FString& Message)
{
    if(!UVFNCalibrationLibrary::IsValidSlot(Slot)) {Message=TEXT("配置名称不合法。");return false;}
    Attempted.Add(Slot);
    if(!UGameplayStatics::DoesSaveGameExist(DiskSlot(Slot),0)) {Message=TEXT("尚无已保存配置，正在使用节点原参数。");return false;}
    auto* Save=Cast<UVFNCalibrationSave>(UGameplayStatics::LoadGameFromSlot(DiskSlot(Slot),0));
    if(!Save || !UVFNCalibrationLibrary::ValidateProfile(Save->Profile,Message)) {Message=TEXT("配置读取或校验失败，保留当前配置。");return false;}
    UVFNCalibrationLibrary::UpgradeProfile(Save->Profile);
    Profiles.Add(Slot,Save->Profile);Previews.Remove(Slot);++Revision;Message=TEXT("已读取配置：")+Slot.ToString();return true;
}
bool UVFNCalibrationLibrary::SaveProfile(const UObject* Context,FName Slot,const FVFNPerformerProfile& P,FString& M)
{
    if(auto* S=Service(Context)) return S->Save(Slot,P,M);M=TEXT("请在运行中的游戏世界保存个人配置。");return false;
}
bool UVFNCalibrationLibrary::LoadProfile(const UObject* Context,FName Slot,FVFNPerformerProfile& P,FString& M)
{
    if(auto* S=Service(Context)) {if(S->Reload(Slot,M)) if(const auto* R=S->FindProfile(Slot)) {P=*R;return true;}}
    else M=TEXT("请在运行中的游戏世界读取个人配置。");return false;
}

float UVFNCalibrationLibrary::ApplyMorphTweak(float Value,const FVFNMorphTweak& T)
{
    if(!T.bEnabled || !FMath::IsFinite(Value)) return 0.f;
    return FMath::Clamp(Value*FMath::Clamp(T.Gain,0.f,3.f)+FMath::Clamp(T.Offset,-1.f,1.f),FMath::Clamp(T.Minimum,0.f,1.5f),FMath::Clamp(FMath::Max(T.Minimum,T.Maximum),0.f,1.5f));
}
TArray<FName> UVFNCalibrationLibrary::ListProfiles(const UObject* C) {if(auto* S=Service(C))return S->ListProfiles();return {};}
bool UVFNCalibrationLibrary::ActivateProfile(const UObject* C,FName B,FName N,FString& M) {if(auto* S=Service(C))return S->Activate(B,N,M);M=TEXT("请在运行时管理配置。");return false;}
bool UVFNCalibrationLibrary::CopyProfile(const UObject* C,FName F,FName T,FString& M) {if(auto* S=Service(C))return S->CopyProfile(F,T,M);M=TEXT("请在运行时管理配置。");return false;}
bool UVFNCalibrationLibrary::RenameProfile(const UObject* C,FName F,FName T,FString& M) {if(auto* S=Service(C))return S->RenameProfile(F,T,M);M=TEXT("请在运行时管理配置。");return false;}
bool UVFNCalibrationLibrary::DeleteProfile(const UObject* C,FName N,FString& M) {if(auto* S=Service(C))return S->DeleteProfile(N,M);M=TEXT("请在运行时管理配置。");return false;}
bool UVFNCalibrationLibrary::ExportProfile(const UObject* C,FName N,FString& M) {if(auto* S=Service(C))return S->ExportProfile(N,M);M=TEXT("请在运行时管理配置。");return false;}
bool UVFNCalibrationLibrary::ImportProfile(const UObject* C,FName F,FName T,FString& M) {if(auto* S=Service(C))return S->ImportProfile(F,T,M);M=TEXT("请在运行时管理配置。");return false;}
