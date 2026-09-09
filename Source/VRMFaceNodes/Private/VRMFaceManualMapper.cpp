#include "VRMFaceManualMapper.h"
#include "VRMFaceLibrary.h"
#include "LiveLinkRemapAsset.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

UVFNManualMapper::UVFNManualMapper()
{
    // Hard reference includes the self-contained preset when cooking a game.
    static ConstructorHelpers::FClassFinder<ULiveLinkRemapAsset> Preset(TEXT("/VRMFaceNodes/Presets/BP_ManualFacialRemap"));
    PresetClass=Preset.Class;
}
const TArray<FName>& UVFNManualMapper::ExtraNames()
{
    static const TArray<FName> Names={TEXT("Fcl_MTH_O"),TEXT("Fcl_MTH_U"),TEXT("Fcl_MTH_I"),TEXT("Fcl_MTH_E"),TEXT("Fcl_MTH_Fun"),TEXT("Fcl_MTH_Angry")};
    return Names;
}
FVFNFrame UVFNManualMapper::MapFrame(const FVFNFrame& Frame)
{
    FVFNFrame R=Frame;
    if(!ensure(IsInGameThread()) || !Frame.bValid || !Frame.bMetaHuman || Frame.SourceCurves.IsEmpty() || !PresetClass) return R;
    if(!Remapper || Remapper->GetClass()!=PresetClass)
    {
        Remapper=NewObject<ULiveLinkRemapAsset>(this,PresetClass);Names.Empty();
        if(auto* P=FindFProperty<FBoolProperty>(PresetClass,TEXT("映射方式")))P->SetPropertyValue_InContainer(Remapper,true);
        // Preserve the author's tuned operating mode. In this Blueprint, false
        // actually takes the mirrored branch; normalize its OUTPUT below.
        if(auto* P=FindFProperty<FBoolProperty>(PresetClass,TEXT("镜像")))P->SetPropertyValue_InContainer(Remapper,false);
    }
    TMap<FName,float> M;
    for(const auto& P:Frame.SourceCurves)
    {
        if(!FMath::IsFinite(P.Value))continue;
        FName* N=Names.Find(P.Key);
        if(!N){Names.Add(P.Key,Remapper->GetRemappedCurveName(P.Key));N=Names.Find(P.Key);}
        M.Add(*N,FMath::Clamp(P.Value,0.f,1.f));
    }
    Remapper->RemapCurveElements(M);
    R.ManualCurves.Empty();
    auto Copy=[&](FName N)
    {
        if(const float* V=M.Find(N))R.ManualCurves.Add(N,FMath::IsFinite(*V)?FMath::Clamp(*V,0.f,1.5f):0.f);
    };
    for(FName N:UVRMFaceLibrary::SemanticNames())Copy(N);
    for(FName N:ExtraNames())Copy(N);
    // The manual eyelid macro writes aliases consumed by its master AnimBP.
    // Publish those shaped values as semantic curves for this node pipeline.
    // Missing aliases are NOT zero-valued observations: the author's master
    // AnimBP handles those eyes separately. Keep the established eye pipeline.
    if(const float* V=M.Find(TEXT("左眼闭合")))R.ManualCurves.Add(TEXT("EyeBlinkLeft"),FMath::Clamp(*V,0.f,1.5f));
    if(const float* V=M.Find(TEXT("右眼闭合")))R.ManualCurves.Add(TEXT("EyeBlinkRight"),FMath::Clamp(*V,0.f,1.5f));
    // Return anatomical channels to calibration. The central mirror then
    // reproduces the author's original output when enabled, including the
    // asymmetric left/right mouth ranges; disabling it reflects the whole face.
    const TMap<FName,float> Mirrored=R.ManualCurves;
    for(FName N:UVRMFaceLibrary::SemanticNames())
    {
        const FString Name=N.ToString();if(!Name.EndsWith(TEXT("Left")))continue;
        const FName Other(*(Name.LeftChop(4)+TEXT("Right")));
        R.ManualCurves.Remove(N);R.ManualCurves.Remove(Other);
        if(const float* V=Mirrored.Find(N))R.ManualCurves.Add(Other,*V);
        if(const float* V=Mirrored.Find(Other))R.ManualCurves.Add(N,*V);
    }
    // Direct ARKit precedence is resolved AFTER restoring anatomical sides.
    for(FName N:UVRMFaceLibrary::SemanticNames())if(Frame.SourceCurves.Contains(N))R.ManualCurves.Remove(N);
    // Fix the manual Clamp's negative upper bound at rest, without adding closure.
    if(float* Close=R.ManualCurves.Find(TEXT("MouthClose")))
        *Close=FMath::Clamp(*Close,0.f,FMath::Max((R.ManualCurves.Contains(TEXT("JawOpen"))?R.ManualCurves:Frame.Curves).FindRef(TEXT("JawOpen"))-.1f,0.f));
    return R;
}
