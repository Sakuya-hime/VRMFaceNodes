#include "VRMFaceLibrary.h"
#include "VRMFaceManualMapper.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "Roles/LiveLinkBasicRole.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkAnimationTypes.h"

namespace
{
    float Finite(float V) { return FMath::IsFinite(V) ? V : 0.f; }
    float Unit(float V) { return FMath::Clamp(Finite(V), 0.f, 1.f); }
    float Read(const TMap<FName,float>& M, FName N) { return Finite(M.FindRef(N)); }
    float Ramp(float V, float A, float B) { return Unit((V-A)/FMath::Max(B-A, .0001f)); }
    float Smooth(float V) { V=Unit(V); return V*V*(3.f-2.f*V); }
    bool ValidDelta(float D) { return FMath::IsFinite(D) && D>0.f && D<.25f; }
    struct FContribution { const TCHAR* Source; const TCHAR* Target; float Weight; };
    // Semantic channel table adapted from VRM4U (MIT); attribution in ThirdPartyNotices.md.
    const FContribution Contributions[] = {
#include "VRMFaceContributions.inl"
    };
    FString Key(FString S)
    {
        S=S.ToLower().Replace(TEXT("_"),TEXT("")).Replace(TEXT("-"),TEXT("")).Replace(TEXT(" "),TEXT(""));
        return S;
    }
    FName FindMorph(const TArray<FName>& Available, FName Canonical)
    {
        if (Available.Contains(Canonical)) return Canonical;
        FString S=Canonical.ToString();
        TArray<FString> Aliases={Key(S),Key(S.Replace(TEXT("Left"),TEXT("_L"),ESearchCase::IgnoreCase).Replace(TEXT("Right"),TEXT("_R"),ESearchCase::IgnoreCase))};
        FName Found;
        for (FName N:Available)
        {
            if (Aliases.Contains(Key(N.ToString())))
            {
                if (!Found.IsNone() && Found!=N) return NAME_None; // ambiguous aliases are never guessed
                Found=N;
            }
        }
        return Found;
    }
    FName FindBone(const FReferenceSkeleton& Ref, std::initializer_list<const TCHAR*> Names)
    {
        for (const TCHAR* N:Names) if (Ref.FindBoneIndex(FName(N))!=INDEX_NONE) return FName(N);
        return NAME_None;
    }
}

const TArray<FName>& UVRMFaceLibrary::SemanticNames()
{
    static const TArray<FName> Names=[]()
    {
        TArray<FName> R;
        for (const auto& C:Contributions) R.AddUnique(FName(C.Target));
        return R;
    }();
    return Names;
}

FVFNModelProfile UVRMFaceLibrary::InspectModel(USkeletalMesh* Model)
{
    FVFNModelProfile P;
    if (!IsValid(Model)) { P.Notes.Add(TEXT("尚未指定骨骼网格体。")); return P; }
    TArray<FName> Morphs;
    for (const UMorphTarget* Morph:Model->GetMorphTargets()) if (Morph) Morphs.Add(Morph->GetFName());
    for (FName Name:SemanticNames())
    {
        FName Match=FindMorph(Morphs,Name);
        if (Match.IsNone())
        {
            if (Name==TEXT("EyeBlinkLeft")) Match=FindMorph(Morphs,TEXT("Blink_L"));
            else if (Name==TEXT("EyeBlinkRight")) Match=FindMorph(Morphs,TEXT("Blink_R"));
            else if (Name==TEXT("JawOpen")) Match=FindMorph(Morphs,TEXT("A"));
        }
        if (!Match.IsNone()) P.MorphMap.Add(Name,Match); else P.MissingCurves.Add(Name);
    }
    P.MatchedCount=P.MorphMap.Num();
    for(FName N:UVFNManualMapper::ExtraNames())if(Morphs.Contains(N))P.MorphMap.Add(N,N);
    const auto& Ref=Model->GetRefSkeleton();
    P.bVRoid=Ref.FindBoneIndex(TEXT("J_Bip_C_Head"))!=INDEX_NONE;
    P.Head=FindBone(Ref,{TEXT("J_Bip_C_Head"),TEXT("head"),TEXT("Head")});
    P.Chest=FindBone(Ref,{TEXT("J_Bip_C_UpperChest"),TEXT("J_Bip_C_Chest"),TEXT("spine_03"),TEXT("chest"),TEXT("Chest")});
    P.LeftEye=FindBone(Ref,{TEXT("J_Adj_L_FaceEye"),TEXT("eye_l"),TEXT("LeftEye")});
    P.RightEye=FindBone(Ref,{TEXT("J_Adj_R_FaceEye"),TEXT("eye_r"),TEXT("RightEye")});
    P.LeftUpperArm=FindBone(Ref,{TEXT("J_Bip_L_UpperArm")});
    P.RightUpperArm=FindBone(Ref,{TEXT("J_Bip_R_UpperArm")});
    P.TeethMorph=FindMorph(Morphs,TEXT("VRoid_TeethRetract"));
    P.ChinMorph=FindMorph(Morphs,TEXT("VFN_ChinAssist"));
    P.MouthInnerMorph=FindMorph(Morphs,TEXT("VRoid_MouthInnerRetract"));
    P.Notes.Add(FString::Printf(TEXT("已识别 %d / %d 项标准表情；缺失项保留原动画，不生成不存在的形态键。"),P.MatchedCount,SemanticNames().Num()));
    if (P.TeethMorph.IsNone()) P.Notes.Add(TEXT("没有牙齿避让形态键；牙齿避让节点会跳过几何修正。可用编辑器配置节点为已验证的模型添加修正。"));
    if (P.LeftEye.IsNone() || P.RightEye.IsNone()) P.Notes.Add(TEXT("未完整识别眼球骨骼：使用已有眼球形态键，或在节点里指定骨骼。"));
    if (P.bVRoid) P.Notes.Add(TEXT("VRoid 预设是调校起点，不代表所有 VRoid 模型都需要相同闭眼和嘴部幅度。"));
    return P;
}

FVFNSettings UVRMFaceLibrary::ResolveSettings(const FVFNSettings& S, const FVFNModelProfile& P)
{
    FVFNSettings R=S;
    if (S.bAutomaticPreset && !P.bVRoid)
    {
        R.bCalibration=false; R.bVRoidAxes=false; R.bSwapEyes=false;
        R.BlinkStart=0.f; R.BlinkEnd=1.f; R.BlinkGain=1.f; R.PuckerLimit=1.f;
        R.LipRollStart=0.f; R.LipRollEnd=1.f; R.LipRollLimit=1.f; R.bPuckerSmoothstep=false;
    }
    return R;
}

FVFNFrame UVRMFaceLibrary::NormalizeCurves(const TMap<FName,float>& Source)
{
    FVFNFrame R;
    R.bValid=!Source.IsEmpty();
    R.SourceCurves=Source;
    for (const auto& C:Contributions)
        if (const float* V=Source.Find(FName(C.Source))) { R.Curves.FindOrAdd(FName(C.Target))+=Unit(*V)*C.Weight; R.bMetaHuman=true; }
    // Direct ARKit channels win over a simultaneous MetaHuman representation.
    for (FName N:SemanticNames()) if (const float* V=Source.Find(N)) R.Curves.Add(N,Unit(*V));
    // Preserve the extra MetaHuman range for newly calibrated three-point jaws.
    // A simultaneous direct ARKit JawOpen keeps precedence, including its range.
    if (Source.Contains(TEXT("CTRL_expressions_jawOpen")) && !Source.Contains(TEXT("JawOpen")))
        if (const float* V=Source.Find(TEXT("CTRL_expressions_jawOpenExtreme")))
            if(FMath::IsFinite(*V)){R.bHasJawOpenExtreme=true;R.JawOpenExtreme=Unit(*V);}
    // Calibrated MetaHuman pucker uses the strongest lip quadrant, as in the user's mapping.
    if (R.bMetaHuman && !Source.Contains(TEXT("MouthPucker")))
    {
        float Pucker=0.f; bool Found=false;
        for (const TCHAR* Stem:{TEXT("mouthLipsPurse"),TEXT("mouthLipsPush")})
            for (const TCHAR* Side:{TEXT("UL"),TEXT("UR"),TEXT("DL"),TEXT("DR")})
                if (const float* V=Source.Find(FName(FString(TEXT("CTRL_expressions_"))+Stem+Side))) { Pucker=FMath::Max(Pucker,Unit(*V)); Found=true; }
        if (Found) R.Curves.Add(TEXT("MouthPucker"),Pucker);
    }
    // Upper-lip correction also uses the strongest side, avoiding a one-sided V-shaped peak.
    if (R.bMetaHuman && !Source.Contains(TEXT("MouthRollUpper")))
        R.Curves.Add(TEXT("MouthRollUpper"),FMath::Max(Read(Source,TEXT("CTRL_expressions_mouthUpperLipRollInL")),Read(Source,TEXT("CTRL_expressions_mouthUpperLipRollInR"))));
    R.Head=FRotator(Read(Source,TEXT("HeadPitch")),Read(Source,TEXT("HeadYaw")),Read(Source,TEXT("HeadRoll")));
    R.bHasHead=Source.Contains(TEXT("HeadPitch")) || Source.Contains(TEXT("HeadYaw")) || Source.Contains(TEXT("HeadRoll"));
    return R;
}

FVFNFrame UVRMFaceLibrary::ReadLiveLink(FName Subject)
{
    auto& Features=IModularFeatures::Get();
    if (!Features.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName)) return {};
    auto& Client=Features.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
    if (Subject.IsNone())
    {
        const auto Subjects=Client.GetSubjects(false,true);
        if (Subjects.Num()!=1) return {}; // no silent choice between multiple performers
        Subject=Subjects[0].SubjectName.Name;
    }
    FLiveLinkSubjectFrameData Data;
    if (!Client.EvaluateFrame_AnyThread(FLiveLinkSubjectName(Subject),ULiveLinkBasicRole::StaticClass(),Data))
        if (!Client.EvaluateFrame_AnyThread(FLiveLinkSubjectName(Subject),ULiveLinkAnimationRole::StaticClass(),Data)) return {};
    const auto* Static=Data.StaticData.Cast<FLiveLinkBaseStaticData>();
    const auto* Frame=Data.FrameData.Cast<FLiveLinkBaseFrameData>();
    if (!Static || !Frame) return {};
    TMap<FName,float> Raw;
    for (int32 I=0; I<FMath::Min(Static->PropertyNames.Num(),Frame->PropertyValues.Num()); ++I) Raw.Add(Static->PropertyNames[I],Finite(Frame->PropertyValues[I]));
    FVFNFrame Result=NormalizeCurves(Raw);
    Result.SourceTimestamp=Frame->WorldTime.GetSourceTime();
    return Result;
}

float UVRMFaceLibrary::TimeAlpha(float Seconds, float DeltaSeconds)
{
    if (!ValidDelta(DeltaSeconds)) return 1.f;
    if (!FMath::IsFinite(Seconds) || Seconds<=.00001f) return 1.f;
    return 1.f-FMath::Exp(-DeltaSeconds/Seconds);
}

FRotator UVRMFaceLibrary::EyeRotation(const FVFNFrame& Frame, const FVFNSettings& S, bool bLeft)
{
    const FString Side=bLeft?TEXT("Left"):TEXT("Right");
    const auto V=[&](const TCHAR* Stem){return Read(Frame.Curves,FName(FString(Stem)+Side));};
    const float Degrees=FMath::Clamp(Finite(S.EyeDegrees),0.f,40.f);
    const float Vertical=Degrees*(V(TEXT("EyeLookDown"))-V(TEXT("EyeLookUp")));
    const float Horizontal=Degrees*(bLeft?1.f:-1.f)*(V(TEXT("EyeLookOut"))-V(TEXT("EyeLookIn")));
    // The VRoid +Y-facing rig has the opposite component yaw convention.
    // With mirrored sides this matches the user's manual eye graph:
    // source Right -> left bone, source Left -> right bone, yaw=LookLeft-LookRight.
    // This is an axis conversion, not a second mirror; vertical gaze is unchanged.
    return S.bVRoidAxes?FRotator(0.,-Horizontal,Vertical):FRotator(Vertical,Horizontal,0.);
}

FVFNFrame UVRMFaceLibrary::ApplyManualBaseline(const FVFNFrame& Frame)
{
    FVFNFrame R=Frame;
    for(const auto& P:Frame.ManualCurves)
    {
        if(Frame.PerformerCalibratedChannels.Contains(P.Key))continue;
        R.Curves.Add(P.Key,P.Value);R.ManualMappedChannels.AddUnique(P.Key);
    }
    if(R.ManualMappedChannels.Contains(TEXT("MouthClose")))
        R.Curves.FindOrAdd(TEXT("MouthClose"))=FMath::Clamp(R.Curves.FindRef(TEXT("MouthClose")),0.f,FMath::Max(R.Curves.FindRef(TEXT("JawOpen"))-.1f,0.f));
    R.ManualCurves.Empty();R.SourceCurves.Empty();
    return R;
}

FVFNFrame UVRMFaceLibrary::Calibrate(const FVFNFrame& Frame, const FVFNSettings& S, FVFNState& State, float Dt)
{
    FVFNFrame R=Frame;
    if (S.bMirrorCapture)
    {
        // Swap every anatomical side once, including gaze.
        // Never combine this with the legacy eye-only correction below.
        for(FName N:UVRMFaceLibrary::SemanticNames())
        {
            const FString Name=N.ToString();
            if(!Name.EndsWith(TEXT("Left")))continue;
            const FName Other(*(Name.LeftChop(4)+TEXT("Right")));
            const float* L=Frame.Curves.Find(N);const float* RR=Frame.Curves.Find(Other);
            R.Curves.Remove(N);R.Curves.Remove(Other);
            if(L)R.Curves.Add(Other,*L);if(RR)R.Curves.Add(N,*RR);
            R.ManualMappedChannels.Remove(N);R.ManualMappedChannels.Remove(Other);
            if(Frame.ManualMappedChannels.Contains(N))R.ManualMappedChannels.AddUnique(Other);
            if(Frame.ManualMappedChannels.Contains(Other))R.ManualMappedChannels.AddUnique(N);
            R.PerformerCalibratedChannels.Remove(N);R.PerformerCalibratedChannels.Remove(Other);
            if(Frame.PerformerCalibratedChannels.Contains(N))R.PerformerCalibratedChannels.AddUnique(Other);
            if(Frame.PerformerCalibratedChannels.Contains(Other))R.PerformerCalibratedChannels.AddUnique(N);
        }
    }
    else if (S.bSwapEyes)
    {
        for (const TCHAR* Stem:{TEXT("EyeBlink"),TEXT("EyeLookDown"),TEXT("EyeLookIn"),TEXT("EyeLookOut"),TEXT("EyeLookUp"),TEXT("EyeSquint"),TEXT("EyeWide")})
        {
            FName L(FString(Stem)+TEXT("Left")), RR(FString(Stem)+TEXT("Right"));
            const float* A=Frame.Curves.Find(L); const float* B=Frame.Curves.Find(RR);
            if (A && B) {R.Curves.Add(L,*B);R.Curves.Add(RR,*A);}
            R.ManualMappedChannels.Remove(L);R.ManualMappedChannels.Remove(RR);
            if(Frame.ManualMappedChannels.Contains(L))R.ManualMappedChannels.AddUnique(RR);if(Frame.ManualMappedChannels.Contains(RR))R.ManualMappedChannels.AddUnique(L);
            const bool CL=Frame.PerformerCalibratedChannels.Contains(L),CR=Frame.PerformerCalibratedChannels.Contains(RR);
            R.PerformerCalibratedChannels.Remove(L);R.PerformerCalibratedChannels.Remove(RR);
            if(CL) R.PerformerCalibratedChannels.AddUnique(RR);if(CR) R.PerformerCalibratedChannels.AddUnique(L);
        }
        // Swapping eyes must preserve world-relative gaze, not reverse inward/outward.
        for (const TCHAR* Side:{TEXT("Left"),TEXT("Right")})
        {
            FName In(FString(TEXT("EyeLookIn"))+Side), Out(FString(TEXT("EyeLookOut"))+Side);
            const float A=R.Curves.FindRef(In),B=R.Curves.FindRef(Out);
            if(R.Curves.Contains(In) && R.Curves.Contains(Out)) {R.Curves.Add(In,B);R.Curves.Add(Out,A);}
        }
    }
    // The VRoid preset follows the user's working HeadPosition graph:
    // component (Pitch,Yaw,Roll) = (-HeadPitch*gain, HeadRoll*gain, HeadYaw).
    // That graph already supplies the desired front-camera mirror orientation.
    // HeadYaw becomes component Roll (nodding here), so never negate it for a
    // horizontal mirror. The opposite view reflects component X, negating
    // component Pitch/Yaw only. Generic +X-facing rigs reflect component Y.
    if(S.bVRoidAxes)
    {
        if(!S.bMirrorCapture) {R.Head.Pitch=-R.Head.Pitch;R.Head.Roll=-R.Head.Roll;}
    }
    else if(S.bMirrorCapture) {R.Head.Yaw=-R.Head.Yaw;R.Head.Roll=-R.Head.Roll;}
    if (!S.bCalibration) return R;
    for (FName N:{FName(TEXT("EyeBlinkLeft")),FName(TEXT("EyeBlinkRight"))})
    {
        if (float* V=R.Curves.Find(N))
        {
            const bool Personal=R.PerformerCalibratedChannels.Contains(N);
            const float Target=R.ManualMappedChannels.Contains(N)?FMath::Clamp(*V,0.f,1.5f):Smooth(Ramp(*V,Personal?0.f:S.BlinkStart,Personal?1.f:S.BlinkEnd))*FMath::Clamp(S.BlinkGain,0.f,1.5f);
            float& Prev=State.BlinkHistory.FindOrAdd(N);
            Prev=FMath::Lerp(Prev,Target,TimeAlpha(1.f/FMath::Max(Target>Prev?S.BlinkCloseRate:S.BlinkOpenRate,.001f),Dt));
            *V=Prev;
        }
    }
    for(FName N:{FName(TEXT("MouthSmileLeft")),FName(TEXT("MouthSmileRight"))}) if(float* V=R.Curves.Find(N)) *V*=FMath::Clamp(S.SmileGain,0.f,1.5f);
    if (float* V=R.Curves.Find(TEXT("MouthPucker"))) { *V=FMath::Clamp(*V,0.f,Unit(S.PuckerLimit)); if (S.bPuckerSmoothstep && !R.ManualMappedChannels.Contains(TEXT("MouthPucker"))) *V=Smooth(*V); }
    if (!R.PerformerCalibratedChannels.Contains(TEXT("JawOpen")) && !R.ManualMappedChannels.Contains(TEXT("JawOpen")))
        if(float* V=R.Curves.Find(TEXT("JawOpen"))) *V=Ramp(*V,FMath::Clamp(S.JawRestThreshold,0.f,.3f),1.f);
    if (!R.ManualMappedChannels.Contains(TEXT("MouthClose"))) if (float* V=R.Curves.Find(TEXT("MouthClose"))) *V=FMath::Min(Unit(*V),FMath::Clamp(Read(R.Curves,TEXT("JawOpen"))-.1f,0.f,.2f))*Unit(S.MouthCloseStrength);
    if (!R.ManualMappedChannels.Contains(TEXT("MouthRollUpper"))) if (float* V=R.Curves.Find(TEXT("MouthRollUpper"))) *V=Ramp(*V,S.LipRollStart,S.LipRollEnd)*Unit(S.LipRollLimit);
    return R;
}

FVFNFrame UVRMFaceLibrary::RetractTeeth(const FVFNFrame& Frame, const FVFNSettings& S, FVFNState& State, float Dt)
{
    FVFNFrame R=Frame;
    const float Target=S.bTeeth?Smooth(Ramp(FMath::Max(Read(Frame.Curves,TEXT("MouthLeft")),Read(Frame.Curves,TEXT("MouthRight"))),S.TeethStart,S.TeethEnd))*Unit(S.TeethAmount):0.f;
    State.TeethWeight=!S.bTeeth?0.f:S.bInstantTeeth && Target>State.TeethWeight?Target:FMath::Lerp(State.TeethWeight,Target,TimeAlpha(1.f/FMath::Max(S.TeethRate,.001f),Dt));
    R.Curves.Add(TEXT("VRoid_TeethRetract"),State.TeethWeight);
    R.Curves.Add(TEXT("VRoid_MouthInnerRetract"),State.TeethWeight);
    return R;
}

FVFNFrame UVRMFaceLibrary::Stabilize(const FVFNFrame& Frame, const FVFNSettings& S, FVFNState& State, float Dt)
{
    FVFNFrame R=Frame;
    float Speed=0.f;
    if (Frame.bHasHead && ValidDelta(Dt) && State.bHeadInitialized)
    {
        const FRotator D=(Frame.Head-State.PreviousHead).GetNormalized();
        Speed=FVector(D.Pitch,D.Yaw,D.Roll).Size()/Dt;
        if (S.bHeadRadians) Speed=FMath::RadiansToDegrees(Speed);
    }
    State.PreviousHead=Frame.Head;
    State.bHeadInitialized=Frame.bHasHead && ValidDelta(Dt);
    State.MotionProtection=S.bStabilize && ValidDelta(Dt)?FMath::Max(Smooth(Ramp(Speed,S.MotionStart,S.MotionFull)),State.MotionProtection*(1.f-TimeAlpha(S.RecoverySeconds,Dt))):0.f;
    const bool CanFilter=S.bStabilize && S.bHoldMotionExpressions && Frame.bValid && Frame.bHasHead && ValidDelta(Dt);
    State.bMotionFiltering=CanFilter && State.MotionProtection>=.01f;
    const float FilterWeight=State.bMotionFiltering?Smooth(Ramp(State.MotionProtection,.01f,.35f)):0.f;
    for (auto& Pair:R.Curves)
    {
        FString N=Pair.Key.ToString().ToLower();
        if(N==TEXT("vroid_teethretract") || N==TEXT("vroid_mouthinnerretract")) continue; // Corrective attack must never lag behind the mouth.
        float Still=0.f, Moving=0.f;
        const bool Blink=N.StartsWith(TEXT("eyeblink"));
        const bool Brow=N.StartsWith(TEXT("brow"));
        const bool Lid=N.StartsWith(TEXT("eyesquint")) || N.StartsWith(TEXT("eyewide"));
        if (Blink) Moving=Pair.Value>.9f?S.IntentionalBlinkSeconds:S.BlinkMoving;
        else if (N.StartsWith(TEXT("mouth")) || N.StartsWith(TEXT("jaw")) || N.StartsWith(TEXT("cheek")) || N.StartsWith(TEXT("nose")) || N.StartsWith(TEXT("tongue")) || N.StartsWith(TEXT("vroid_"))) {Still=S.MouthStill;Moving=S.MouthMoving;}
        else if (N.StartsWith(TEXT("brow"))) {Still=S.BrowStill;Moving=S.BrowMoving;}
        else if (N.StartsWith(TEXT("eyesquint")) || N.StartsWith(TEXT("eyewide"))) {Still=S.LidStill;Moving=S.LidMoving;}
        else continue;
        float* Old=State.FilterHistory.Find(Pair.Key);
        float Target=Pair.Value;
        if(FilterWeight>0.f && (Blink || Brow || Lid))
        {
            const float* Previous=State.MotionInputPrevious.Find(Pair.Key);
            const float* Older=State.MotionInputOlder.Find(Pair.Key);
            if(Previous && Older)
            {
                // A causal median rejects an isolated up OR down spike. A
                // sustained step/ramp passes after one frame, even while moving;
                // no expression must reach full amplitude or wait for a stop.
                const float Median=FMath::Max(FMath::Min(Target,*Previous),FMath::Min(FMath::Max(Target,*Previous),*Older));
                Target=FMath::Lerp(Target,Median,FilterWeight);
            }
        }
        // Bound the extra motion smoothing, including mouth/jaw speech shapes.
        // Closing, reopening and partial blinks use the same short upper bound.
        Moving=FMath::Min(Moving,FMath::Clamp(S.MotionResponseLimit,0.f,.2f)*(Blink?.5f:1.f));
        const float Alpha=S.bStabilize?TimeAlpha(FMath::Lerp(Still,Moving,State.MotionProtection),Dt):1.f;
        const float Value=Old?FMath::Lerp(*Old,Target,Alpha):Target;
        State.FilterHistory.Add(Pair.Key,Value); Pair.Value=Value;
    }
    if(CanFilter)
    {
        State.MotionInputOlder=MoveTemp(State.MotionInputPrevious);
        State.MotionInputPrevious=Frame.Curves;
    }
    else
    {
        State.MotionInputPrevious.Empty();State.MotionInputOlder.Empty();
    }
    if(R.ManualMappedChannels.Contains(TEXT("MouthClose")))
        R.Curves.FindOrAdd(TEXT("MouthClose"))=FMath::Clamp(R.Curves.FindRef(TEXT("MouthClose")),0.f,FMath::Max(R.Curves.FindRef(TEXT("JawOpen"))-.1f,0.f));
    // During release the displayed mouth can lag behind the raw mouth. Maintain
    // sufficient retraction for that displayed pose until it actually returns.
    if(S.bTeeth)
    {
        const float Floor=Smooth(Ramp(FMath::Max(Read(R.Curves,TEXT("MouthLeft")),Read(R.Curves,TEXT("MouthRight"))),S.TeethStart,S.TeethEnd))*Unit(S.TeethAmount);
        for(FName N:{FName(TEXT("VRoid_TeethRetract")),FName(TEXT("VRoid_MouthInnerRetract"))})
            if(float* V=R.Curves.Find(N)) *V=FMath::Max(*V,Floor);
    }
    return R;
}

void UVRMFaceLibrary::FollowHead(const FVFNFrame& Frame, const FVFNSettings& S, FVFNState& State, float Dt)
{
    FRotator Target=Frame.bHasHead && S.bHead?Frame.Head:FRotator::ZeroRotator;
    if (S.bHeadRadians) Target*=180.f/PI;
    if (S.bVRoidAxes) Target=FRotator(-Target.Pitch*S.HeadGain,Target.Roll*S.HeadGain,Target.Yaw);
    else Target*=S.HeadGain;
    Target.Pitch=FMath::Clamp(FRotator::NormalizeAxis(Target.Pitch),-35.,35.);
    Target.Yaw=FMath::Clamp(FRotator::NormalizeAxis(Target.Yaw),-60.,60.);
    Target.Roll=FMath::Clamp(FRotator::NormalizeAxis(Target.Roll),-45.,45.);
    if (!State.bFollowInitialized || !ValidDelta(Dt)) {State.SmoothedHead=Target;State.SmoothedBody=Target*Unit(S.BodyGain);}
    else
    {
        State.SmoothedHead=FMath::RInterpTo(State.SmoothedHead,Target,Dt,S.HeadRate);
        State.SmoothedBody=FMath::RInterpTo(State.SmoothedBody,State.SmoothedHead*Unit(S.BodyGain),Dt,S.BodyRate);
    }
    State.bFollowInitialized=true;
}
void UVRMFaceLibrary::ResetState(FVFNState& State) { State=FVFNState(); }
