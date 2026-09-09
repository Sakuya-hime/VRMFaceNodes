#include "AnimNode_VRMFace.h"
#include "VRMFaceManualMapper.h"
#include "VRMFaceLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
    FName VFNInternalName(FName N) { return FName(FString(TEXT("VFN_"))+N.ToString()); }
    bool IsInput(EVFNStage S) { return S==EVFNStage::Automatic || S==EVFNStage::Input; }
    bool At(EVFNStage S, EVFNStage Pass) { return S==EVFNStage::Automatic || S==Pass; }
    void RotateCS(FCSPose<FCompactPose>& Pose, FName Bone, const FRotator& Rotation)
    {
        if (Bone.IsNone() || Rotation.IsNearlyZero()) return;
        FBoneReference Ref; Ref.BoneName=Bone; Ref.Initialize(Pose.GetPose().GetBoneContainer());
        if (!Ref.IsValidToEvaluate(Pose.GetPose().GetBoneContainer())) return;
        FCompactPoseBoneIndex Id=Ref.GetCompactPoseIndex(Pose.GetPose().GetBoneContainer());
        FTransform TM=Pose.GetComponentSpaceTransform(Id);
        TM.SetRotation((Rotation.Quaternion()*TM.GetRotation()).GetNormalized());
        TArray<FBoneTransform> One; One.Emplace(Id,TM);
        Pose.LocalBlendCSBoneTransforms(One,1.f);
    }
}

void FAnimNode_VRMFace::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    SourcePose.Initialize(Context); State=FVFNState(); InputFrame={}; LastLiveFrame={};
    bModelReady=false; bNeedsStep=true; LostSeconds=0.f;LastTimestamp=0.;RepeatedFrameSeconds=0.f;
}
void FAnimNode_VRMFace::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) { SourcePose.CacheBones(Context); }
void FAnimNode_VRMFace::PreUpdate(const UAnimInstance* Instance)
{
    USkeletalMesh* Mesh=Instance && Instance->GetSkelMeshComponent()?Instance->GetSkelMeshComponent()->GetSkeletalMeshAsset():nullptr;
    if (!bModelReady || CachedModel.Get()!=Mesh)
    {
        CachedModel=Mesh; Profile=UVRMFaceLibrary::InspectModel(Mesh); bModelReady=true;
        State=FVFNState(); LastLiveFrame={}; LostSeconds=0.f;LastTimestamp=0.;RepeatedFrameSeconds=0.f;
    }
    Effective=UVRMFaceLibrary::ResolveSettings(Settings,Profile);
    bHasPerformerProfile=false;
    if(Instance && Instance->GetWorld() && Instance->GetWorld()->GetGameInstance())
    {
        auto* Service=Instance->GetWorld()->GetGameInstance()->GetSubsystem<UVFNCalibrationSubsystem>();
        const auto* P=Service->FindProfile(PerformerProfileSlot);
        if(ProfileRevision!=Service->GetRevision() || CachedProfileSlot!=PerformerProfileSlot)
        {
            bool Reset=CachedProfileSlot!=PerformerProfileSlot || !P;
            if(P)
            {
                Reset|=P->bEnhancementsEnabled!=PerformerProfile.bEnhancementsEnabled || P->bCameraCentered!=PerformerProfile.bCameraCentered;
                Reset|=P->bUsePersonalJawWithManual!=PerformerProfile.bUsePersonalJawWithManual || P->bMetaHuman!=PerformerProfile.bMetaHuman || P->bMirrorCapture!=PerformerProfile.bMirrorCapture || P->bOverrideMirror!=PerformerProfile.bOverrideMirror || P->bRecenterHead!=PerformerProfile.bRecenterHead || !P->NeutralHead.Equals(PerformerProfile.NeutralHead) || P->Ranges.Num()!=PerformerProfile.Ranges.Num();
                if(!Reset)for(const auto& Range:P->Ranges)
                {const auto* Old=PerformerProfile.Ranges.Find(Range.Key);if(!Old || !FVFNCurveRange::StaticStruct()->CompareScriptStruct(&Range.Value,Old,PPF_None)){Reset=true;break;}}
            }
            if(Reset){State=FVFNState();LastLiveFrame={};LostSeconds=0.f;}
            ProfileRevision=Service->GetRevision();CachedProfileSlot=PerformerProfileSlot;
        }
        if(P) {PerformerProfile=*P;bHasPerformerProfile=true;}
        if(bHasPerformerProfile && PerformerProfile.bOverrideMirror) {Effective.bMirrorCapture=PerformerProfile.bMirrorCapture;Effective.bSwapEyes=false;}
        if(bHasPerformerProfile && PerformerProfile.bMotionMeasured) {Effective.MotionStart=PerformerProfile.MotionStart;Effective.MotionFull=PerformerProfile.MotionFull;}
        if(bHasPerformerProfile)
        {
            Effective.bEnhancementsEnabled &= PerformerProfile.bEnhancementsEnabled;
            Effective.SpeechAmount=PerformerProfile.SpeechAmount;Effective.ChinStrength=PerformerProfile.ChinStrength;
        }
    }
    if(!Effective.bEnhancementsEnabled)
    {
        Effective.bUseManualMapping=false;Effective.bCalibration=false;Effective.bStabilize=false;
        Effective.bTeeth=false;Effective.ChinStrength=0.f;Effective.BodyGain=0.f;
        Effective.HeadRate=0.f;Effective.BodyRate=0.f;
    }
    if (IsInput(Stage))
    {
        if (CachedSubject!=Subject) {State=FVFNState();LastLiveFrame={};LostSeconds=0.f;CachedSubject=Subject;LastTimestamp=0.;RepeatedFrameSeconds=0.f;}
        InputFrame=bUseTestFrame?TestFrame:UVRMFaceLibrary::ReadLiveLink(Subject);
        if(Effective.bUseManualMapping && Effective.bCalibration && InputFrame.bValid)
        {
            if(!ManualMapper)ManualMapper=NewObject<UVFNManualMapper>(const_cast<UAnimInstance*>(Instance));
            InputFrame=ManualMapper->MapFrame(InputFrame);
        }
        if(!bUseTestFrame && InputFrame.bValid && InputFrame.SourceTimestamp>0.)
        {
            if(InputFrame.SourceTimestamp==LastTimestamp) RepeatedFrameSeconds+=FMath::Max(0.f,DeltaSeconds);
            else RepeatedFrameSeconds=0.f;
            LastTimestamp=InputFrame.SourceTimestamp;
            if(RepeatedFrameSeconds>.5f) InputFrame.bValid=false;
        }
    }
}
void FAnimNode_VRMFace::Update_AnyThread(const FAnimationUpdateContext& Context)
{
    GetEvaluateGraphExposedInputs().Execute(Context);
    SourcePose.Update(Context);
    DeltaSeconds=Context.GetDeltaTime(); bNeedsStep=true;
    if (!bEnabled) State=FVFNState();
}

FVFNFrame FAnimNode_VRMFace::ReadPose(const FPoseContext& Pose) const
{
    FVFNFrame F;
    for (FName Name:UVRMFaceLibrary::SemanticNames())
    {
        bool Has=false; float Value=Pose.Curve.Get(VFNInternalName(Name),Has);
        if (!Has)
        {
            FName Actual=Profile.MorphMap.FindRef(Name);
            if (const FName* Custom=Settings.MorphOverrides.Find(Name)) Actual=*Custom;
            if (!Actual.IsNone()) Value=Pose.Curve.Get(Actual,Has);
        }
        if (Has) F.Curves.Add(Name,Value);
        if(Pose.Curve.Get(FName(FString(TEXT("VFN_Personal_"))+Name.ToString()))>.5f) F.PerformerCalibratedChannels.Add(Name);
        if(Pose.Curve.Get(FName(FString(TEXT("VFN_Manual_"))+Name.ToString()))>.5f) F.ManualMappedChannels.Add(Name);
    }
    TArray<FName> Extras=UVFNManualMapper::ExtraNames();Extras.Add(TEXT("VRoid_TeethRetract"));Extras.Add(TEXT("VRoid_MouthInnerRetract"));
    for (FName N:Extras)
    {
        bool Has=false; float V=Pose.Curve.Get(VFNInternalName(N),Has);
        if (Has) F.Curves.Add(N,V);
    }
    F.Head=FRotator(Pose.Curve.Get(TEXT("VFN_HeadPitch")),Pose.Curve.Get(TEXT("VFN_HeadYaw")),Pose.Curve.Get(TEXT("VFN_HeadRoll")));
    F.bHasHead=Pose.Curve.Get(TEXT("VFN_HasHead"))>.5f;
    bool HasConnection=false;
    const float Connected=Pose.Curve.Get(TEXT("VFN_Connected"),HasConnection);
    F.bValid=HasConnection?Connected>.5f:!F.Curves.IsEmpty();
    return F;
}

void FAnimNode_VRMFace::WritePose(FPoseContext& Pose, const FVFNFrame& F) const
{
    // A sparse mirrored frame can remove a side written by an earlier node.
    // Remove only channels already owned by this pipeline; preserve other animation curves.
    TArray<FName> OwnedNames=UVRMFaceLibrary::SemanticNames();OwnedNames.Append(UVFNManualMapper::ExtraNames());
    for(FName Name:OwnedNames)
    {
        if(F.Curves.Contains(Name))continue;
        bool Owned=false;Pose.Curve.Get(VFNInternalName(Name),Owned);
        if(!Owned)continue;
        Pose.Curve.InvalidateCurveWeight(VFNInternalName(Name));
        Pose.Curve.InvalidateCurveWeight(FName(FString(TEXT("VFN_Manual_"))+Name.ToString()));
        Pose.Curve.InvalidateCurveWeight(FName(FString(TEXT("VFN_Personal_"))+Name.ToString()));
        FName Actual=Profile.MorphMap.FindRef(Name);
        if(const FName* Custom=Settings.MorphOverrides.Find(Name))Actual=*Custom;
        if(!Actual.IsNone())Pose.Curve.InvalidateCurveWeight(Actual);
    }
    for (const auto& Pair:F.Curves)
    {
        Pose.Curve.Set(VFNInternalName(Pair.Key),Pair.Value);
        Pose.Curve.Set(FName(FString(TEXT("VFN_Manual_"))+Pair.Key.ToString()),F.ManualMappedChannels.Contains(Pair.Key)?1.f:0.f);
        Pose.Curve.Set(FName(FString(TEXT("VFN_Personal_"))+Pair.Key.ToString()),F.PerformerCalibratedChannels.Contains(Pair.Key)?1.f:0.f);
        FName Actual=Profile.MorphMap.FindRef(Pair.Key);
        if (const FName* Custom=Settings.MorphOverrides.Find(Pair.Key)) Actual=*Custom;
        if (Pair.Key==TEXT("VRoid_TeethRetract")) Actual=Profile.TeethMorph;
        if (Pair.Key==TEXT("VRoid_MouthInnerRetract")) Actual=Profile.MouthInnerMorph;
        if (!Actual.IsNone()) Pose.Curve.Set(Actual,Pair.Value);
    }
    TMap<FName,FVFNMorphTweak> Tweaks;
    if(Effective.bEnhancementsEnabled)Tweaks=Settings.MorphTweaks;
    if(Effective.bEnhancementsEnabled && bHasPerformerProfile && (PerformerProfile.ModelPath.IsEmpty() || (CachedModel.IsValid() && PerformerProfile.ModelPath==CachedModel->GetPathName()))) for(const auto& P:PerformerProfile.MorphTweaks) Tweaks.Add(P.Key,P.Value);
    for(const auto& P:Tweaks)
    {
        if(!CachedModel.IsValid() || !CachedModel->FindMorphTarget(P.Key)) continue;
        bool Mapped=false;for(const auto& C:F.Curves)
        {
            FName Actual=Profile.MorphMap.FindRef(C.Key);if(const auto* Override=Settings.MorphOverrides.Find(C.Key))Actual=*Override;
            if(C.Key==TEXT("VRoid_TeethRetract"))Actual=Profile.TeethMorph;if(C.Key==TEXT("VRoid_MouthInnerRetract"))Actual=Profile.MouthInnerMorph;
            if(Actual==P.Key){Pose.Curve.Set(P.Key,UVFNCalibrationLibrary::ApplyMorphTweak(C.Value,P.Value));Mapped=true;break;}
        }
        if(!Mapped)
        {
            const FName Base(*(TEXT("VFN_Untuned_")+P.Key.ToString()));bool Has=false;float V=Pose.Curve.Get(Base,Has);
            if(!Has){V=Pose.Curve.Get(P.Key);Pose.Curve.Set(Base,V);}
            Pose.Curve.Set(P.Key,UVFNCalibrationLibrary::ApplyMorphTweak(V,P.Value));
        }
    }
    if(!Profile.ChinMorph.IsNone())
    {
        FName Jaw=Profile.MorphMap.FindRef(TEXT("JawOpen"));if(const auto* Override=Settings.MorphOverrides.Find(TEXT("JawOpen")))Jaw=*Override;
        Pose.Curve.Set(Profile.ChinMorph,Effective.bEnhancementsEnabled&&!Jaw.IsNone()?FMath::Clamp(Pose.Curve.Get(Jaw),0.f,1.f)*Effective.ChinStrength:0.f);
    }
    Pose.Curve.Set(TEXT("VFN_HeadPitch"),F.Head.Pitch);
    Pose.Curve.Set(TEXT("VFN_HeadYaw"),F.Head.Yaw);
    Pose.Curve.Set(TEXT("VFN_HeadRoll"),F.Head.Roll);
    Pose.Curve.Set(TEXT("VFN_HasHead"),F.bHasHead?1.f:0.f);
    Pose.Curve.Set(TEXT("VFN_Connected"),F.bValid?1.f:0.f);
    Pose.Curve.Set(TEXT("VFN_MotionProtection"),State.MotionProtection);
}

void FAnimNode_VRMFace::ApplyBones(FPoseContext& Output, const FVFNFrame& Frame)
{
    if (!At(Stage,EVFNStage::HeadBody) && !At(Stage,EVFNStage::Eyes)) return;
    FCSPose<FCompactPose> CS; CS.InitPose(Output.Pose);
    if (At(Stage,EVFNStage::HeadBody))
    {
        RotateCS(CS,Settings.ChestBone.IsNone()?Profile.Chest:Settings.ChestBone,State.SmoothedBody);
        RotateCS(CS,Settings.HeadBone.IsNone()?Profile.Head:Settings.HeadBone,State.SmoothedHead);
        if (Effective.bRelaxArms && Profile.bVRoid)
        {
            RotateCS(CS,Profile.LeftUpperArm,FRotator(-70.,0.,0.));
            RotateCS(CS,Profile.RightUpperArm,FRotator(70.,0.,0.));
        }
    }
    if (At(Stage,EVFNStage::Eyes) && Effective.bEyes)
    {
        RotateCS(CS,Settings.LeftEyeBone.IsNone()?Profile.LeftEye:Settings.LeftEyeBone,
            UVRMFaceLibrary::EyeRotation(Frame,Effective,true));
        RotateCS(CS,Settings.RightEyeBone.IsNone()?Profile.RightEye:Settings.RightEyeBone,
            UVRMFaceLibrary::EyeRotation(Frame,Effective,false));
    }
    FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS),Output.Pose);
}

void FAnimNode_VRMFace::Evaluate_AnyThread(FPoseContext& Output)
{
    SourcePose.Evaluate(Output);
    if (!bEnabled) return;
    if (bNeedsStep)
    {
        FVFNFrame F=IsInput(Stage)?InputFrame:ReadPose(Output);
        if (IsInput(Stage))
        {
            if(bHasPerformerProfile && F.bValid && Effective.bEnhancementsEnabled) F=UVFNCalibrationLibrary::ApplyProfile(F,PerformerProfile);
            F=UVRMFaceLibrary::ApplyManualBaseline(F);
            if (F.bValid) {LastLiveFrame=F;LostSeconds=0.f;}
            else
            {
                LostSeconds+=FMath::IsFinite(DeltaSeconds)?FMath::Max(0.f,DeltaSeconds):0.f;
                F=LastLiveFrame; F.bValid=false;
                const float Fade=1.f-FMath::Clamp((LostSeconds-Effective.LostHoldSeconds)/FMath::Max(Effective.LostFadeSeconds,.01f),0.f,1.f);
                for (auto& Pair:F.Curves) Pair.Value*=Fade;
                F.Head*=Fade;
            }
        }
        if (At(Stage,EVFNStage::Calibration))
        {
            F=UVRMFaceLibrary::Calibrate(F,Effective,State,DeltaSeconds);
            if(Effective.bEnhancementsEnabled)if(float* Jaw=F.Curves.Find(TEXT("JawOpen")))
                *Jaw=UVFNCalibrationLibrary::SpeechResponse(*Jaw,Effective.SpeechAmount);
        }
        if (At(Stage,EVFNStage::Teeth)) F=UVRMFaceLibrary::RetractTeeth(F,Effective,State,DeltaSeconds);
        if (At(Stage,EVFNStage::Stabilization)) F=UVRMFaceLibrary::Stabilize(F,Effective,State,DeltaSeconds);
        if (At(Stage,EVFNStage::HeadBody)) UVRMFaceLibrary::FollowHead(F,Effective,State,DeltaSeconds);
        SolvedFrame=MoveTemp(F); bNeedsStep=false;
    }
    WritePose(Output,SolvedFrame);
    ApplyBones(Output,SolvedFrame);
}

void FAnimNode_VRMFace::GatherDebugData(FNodeDebugData& DebugData)
{
    DebugData.AddDebugItem(FString::Printf(TEXT("VRM Face | %s | mapped %d/52 | protection %.2f"),*Subject.ToString(),Profile.MatchedCount,State.MotionProtection));
    SourcePose.GatherDebugData(DebugData);
}
