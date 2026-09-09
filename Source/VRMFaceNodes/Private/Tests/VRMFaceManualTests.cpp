#include "VRMFaceManualMapper.h"
#include "VRMFaceLibrary.h"
#include "VRMFaceCalibration.h"
#include "LiveLinkRemapAsset.h"
#include "AnimNode_VRMFace.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNManualBaselineTest,"VRMFaceNodes.ManualBaseline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNManualBaselineTest::RunTest(const FString&)
{
    auto* Mapper=NewObject<UVFNManualMapper>();
    TMap<FName,float> Raw={{TEXT("CTRL_expressions_jawOpen"),.2f},{TEXT("CTRL_expressions_jawOpenExtreme"),.5f},
        {TEXT("CTRL_expressions_mouthCheekBlowL"),.31f},{TEXT("CTRL_expressions_mouthCheekBlowR"),.02f},
        {TEXT("CTRL_expressions_mouthLipsTogetherUL"),1.f},{TEXT("CTRL_expressions_mouthUpperLipRollInL"),.6f},
        {TEXT("CTRL_expressions_mouthLipsPushUL"),1.f}};
    auto F=Mapper->MapFrame(UVRMFaceLibrary::NormalizeCurves(Raw));
    TestTrue(TEXT("Bundled blueprint executed for supplied jaw, cheek and lip sources"),F.ManualCurves.Contains(TEXT("JawOpen")) && F.ManualCurves.Contains(TEXT("CheekPuff")) && F.ManualCurves.Contains(TEXT("MouthRollUpper")));
    TestTrue(TEXT("Manual jaw base plus extreme at 30 percent"),FMath::IsNearlyEqual(F.ManualCurves.FindRef(TEXT("JawOpen")),(.35f-.03f)/.97f,.0001f));
    TestEqual(TEXT("Cheek uses stronger side"),F.ManualCurves.FindRef(TEXT("CheekPuff")),.31f);
    FVFNPerformerProfile P;P.bMetaHuman=true;P.SchemaVersion=4;
    FVFNCurveRange Cheek;Cheek.bEnabled=true;Cheek.RestCeiling=.005476f;Cheek.Maximum=.310045f;Cheek.MaxGain=40.f;P.Ranges.Add(TEXT("CheekPuff"),Cheek);
    FVFNCurveRange Jaw;Jaw.bEnabled=true;Jaw.RestCeiling=.06f;Jaw.Maximum=.45f;Jaw.MaxGain=8.f;P.Ranges.Add(TEXT("JawOpen"),Jaw);
    auto C=UVFNCalibrationLibrary::ApplyProfile(F,P);
    TestFalse(TEXT("Existing jaw data preserved but inactive on manual baseline"),C.PerformerCalibratedChannels.Contains(TEXT("JawOpen")));
    TestTrue(TEXT("Existing cheek calibration retained on stronger side"),C.Curves.FindRef(TEXT("CheekPuff"))>.999f);
    auto R=UVRMFaceLibrary::ApplyManualBaseline(C);
    TestTrue(TEXT("Manual markers survive merge"),R.ManualMappedChannels.Contains(TEXT("JawOpen")));
    TestEqual(TEXT("Merge releases raw data"),R.SourceCurves.Num(),0);
    FVFNSettings S;S.bMirrorCapture=false;S.bSwapEyes=false;FVFNState State;
    R=UVRMFaceLibrary::Calibrate(R,S,State,1.f/60.f);
    TestTrue(TEXT("Pucker not shaped twice"),FMath::IsNearlyEqual(R.Curves.FindRef(TEXT("MouthPucker")),.4f));
    TestTrue(TEXT("Upper lip not clamped twice"),FMath::IsNearlyEqual(R.Curves.FindRef(TEXT("MouthRollUpper")),.1f));
    TestTrue(TEXT("Manual mouth close remains bounded and active"),R.Curves.FindRef(TEXT("MouthClose"))>0.f && R.Curves.FindRef(TEXT("MouthClose"))<=R.Curves.FindRef(TEXT("JawOpen"))-.1f+.00001f);
    P.bUsePersonalJawWithManual=true;C=UVFNCalibrationLibrary::ApplyProfile(F,P);
    TestTrue(TEXT("Personal jaw can be enabled without recording again"),C.PerformerCalibratedChannels.Contains(TEXT("JawOpen")));
    Raw[TEXT("CTRL_expressions_jawOpen")]=0.f;Raw[TEXT("CTRL_expressions_jawOpenExtreme")]=0.f;
    R=UVRMFaceLibrary::ApplyManualBaseline(Mapper->MapFrame(UVRMFaceLibrary::NormalizeCurves(Raw)));
    TestEqual(TEXT("No negative close at rest"),R.Curves.FindRef(TEXT("MouthClose")),0.f);
    TestEqual(TEXT("Mouth stays closed with lip contact present"),R.Curves.FindRef(TEXT("JawOpen")),0.f);
    for(int32 I=0;I<60;++I){R=UVRMFaceLibrary::Stabilize(R,S,State,1.f/60.f);TestTrue(TEXT("Compensation cannot overtake closing jaw"),R.Curves.FindRef(TEXT("MouthClose"))<=FMath::Max(R.Curves.FindRef(TEXT("JawOpen"))-.1f,0.f));}
    Raw.Add(TEXT("JawOpen"),.123f);F=Mapper->MapFrame(UVRMFaceLibrary::NormalizeCurves(Raw));
    TestFalse(TEXT("Mixed input preserves direct ARKit precedence"),F.ManualCurves.Contains(TEXT("JawOpen")));
    Raw.Add(TEXT("MouthLeft"),.123f);F=Mapper->MapFrame(UVRMFaceLibrary::NormalizeCurves(Raw));
    TestFalse(TEXT("Mixed lateral ARKit precedence survives normalization"),F.ManualCurves.Contains(TEXT("MouthLeft")));
    F=Mapper->MapFrame(UVRMFaceLibrary::NormalizeCurves({{TEXT("JawOpen"),.2f}}));
    TestEqual(TEXT("Pure ARKit still uses established pipeline"),F.ManualCurves.Num(),0);
    F=Mapper->MapFrame({});TestEqual(TEXT("Empty source gives no synthetic face"),F.ManualCurves.Num(),0);
    Raw.Remove(TEXT("JawOpen"));Raw[TEXT("CTRL_expressions_jawOpen")]=std::numeric_limits<float>::quiet_NaN();
    F=Mapper->MapFrame(UVRMFaceLibrary::NormalizeCurves(Raw));
    for(const auto& V:F.ManualCurves)TestTrue(TEXT("Nonfinite source cannot propagate"),FMath::IsFinite(V.Value) && V.Value>=0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNManualReferenceTest,"VRMFaceNodes.ManualReferenceParity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNManualReferenceTest::RunTest(const FString&)
{
    UClass* Original=LoadClass<ULiveLinkRemapAsset>(nullptr,TEXT("/Game/蓝图/面部/LiveLink面部映射.LiveLink面部映射_C"),nullptr,LOAD_NoWarn);
    if(!Original){AddInfo(TEXT("Original project-only comparison fixture unavailable; portable baseline tests remain active."));return true;}
    auto* Ref=NewObject<ULiveLinkRemapAsset>(GetTransientPackage(),Original);
    if(auto* V=FindFProperty<FBoolProperty>(Original,TEXT("镜像")))V->SetPropertyValue_InContainer(Ref,false);
    if(auto* V=FindFProperty<FBoolProperty>(Original,TEXT("映射方式")))V->SetPropertyValue_InContainer(Ref,true);
    auto* Mapper=NewObject<UVFNManualMapper>();
    struct FContribution{const TCHAR* Source;const TCHAR* Target;float Weight;};
    const FContribution Inputs[]={
#include "../VRMFaceContributions.inl"
    };
    FRandomStream Random(19307);int32 Compared=0;
    for(int32 Sample=0;Sample<128;++Sample)
    {
        TMap<FName,float> Raw,Expected;
        for(const auto& I:Inputs)Raw.Add(I.Source,Sample==0?0.f:Random.FRand());
        Raw.Add(TEXT("CTRL_expressions_jawOpenExtreme"),Sample==0?0.f:Random.FRand());
        for(const auto& V:Raw)Expected.Add(Ref->GetRemappedCurveName(V.Key),V.Value);
        Ref->RemapCurveElements(Expected);
        Expected.Add(TEXT("EyeBlinkLeft"),Expected.FindRef(TEXT("左眼闭合")));
        Expected.Add(TEXT("EyeBlinkRight"),Expected.FindRef(TEXT("右眼闭合")));
        auto Actual=Mapper->MapFrame(UVRMFaceLibrary::NormalizeCurves(Raw));
        for(const auto& V:Actual.ManualCurves)
        {
            FString Name=V.Key.ToString();FName Reference=V.Key;
            if(Name.EndsWith(TEXT("Left")))Reference=FName(Name.LeftChop(4)+TEXT("Right"));
            else if(Name.EndsWith(TEXT("Right")))Reference=FName(Name.LeftChop(5)+TEXT("Left"));
            const float E=FMath::Clamp(Expected.FindRef(Reference),0.f,1.5f);
            if(!TestTrue(*FString::Printf(TEXT("Reference sample %d %s"),Sample,*V.Key.ToString()),FMath::IsNearlyEqual(V.Value,E,.00001f)))return false;
            ++Compared;
        }
    }
    AddInfo(FString::Printf(TEXT("Compared %d curve values with the untouched original manual blueprint."),Compared));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNManualAnimationTest,"VRMFaceNodes.ManualAnimationPipeline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNManualAnimationTest::RunTest(const FString&)
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/VRMFaceNodesDemo/FaceReady/SK_FaceReady.SK_FaceReady"),nullptr,LOAD_NoWarn);
    UClass* AutoClass=LoadClass<UAnimInstance>(nullptr,TEXT("/Game/VRMFaceNodesDemo/Tests/ABP_AutomaticTest.ABP_AutomaticTest_C"),nullptr,LOAD_NoWarn);
    UClass* ModularClass=LoadClass<UAnimInstance>(nullptr,TEXT("/Game/VRMFaceNodesDemo/Tests/ABP_ModularTest.ABP_ModularTest_C"),nullptr,LOAD_NoWarn);
    if(!Mesh || !AutoClass || !ModularClass){AddInfo(TEXT("Project animation fixtures unavailable in portable plugin."));return true;}
    const UWorld::InitializationValues Init=UWorld::InitializationValues().CreateAISystem(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto Make=[&](UClass* Class)
    {
        auto* A=World->SpawnActor<AActor>();auto* C=NewObject<USkeletalMeshComponent>(A);A->AddInstanceComponent(C);A->SetRootComponent(C);
        C->SetDisablePostProcessBlueprint(true);C->SetSkeletalMeshAsset(Mesh);C->SetAnimInstanceClass(Class);C->RegisterComponent();
        return C;
    };
    auto* Auto=Make(AutoClass);auto* Modular=Make(ModularClass);
    auto Set=[&](USkeletalMeshComponent* C,const FVFNFrame& F)
    {
        auto* A=C->GetAnimInstance();int32 Count=0;
        // These project fixtures expose TestFrame through their ProbeFrame variable.
        // Set the upstream value too, so generated pin handlers do not replace the sample.
        if(A)if(auto* Probe=FindFProperty<FStructProperty>(A->GetClass(),TEXT("ProbeFrame")))
            *Probe->ContainerPtrToValuePtr<FVFNFrame>(A)=F;
        if(A)for(TFieldIterator<FStructProperty> It(A->GetClass());It;++It)if(It->Struct==FAnimNode_VRMFace::StaticStruct())
        {
            auto* N=It->ContainerPtrToValuePtr<FAnimNode_VRMFace>(A);
            N->Settings=FVFNSettings();N->Settings.bMirrorCapture=true;N->Settings.bSwapEyes=false;
            N->PerformerProfileSlot=NAME_None;N->bEnabled=true;N->TestFrame=F;N->bUseTestFrame=true;++Count;
        }
        return Count;
    };
    TMap<FName,float> Raw={{TEXT("CTRL_expressions_jawOpen"),.5f},{TEXT("CTRL_expressions_jawOpenExtreme"),.3f},
        {TEXT("CTRL_expressions_mouthLipsTogetherUL"),.8f},{TEXT("CTRL_expressions_mouthCheekBlowL"),.6f},
        {TEXT("CTRL_expressions_mouthCheekBlowR"),.2f},{TEXT("CTRL_expressions_mouthLipsPushUL"),.9f},
        {TEXT("CTRL_expressions_mouthRight"),.6f},{TEXT("CTRL_expressions_mouthLeft"),.1f},
        {TEXT("CTRL_expressions_eyeBlinkL"),1.f},{TEXT("CTRL_expressions_eyeBlinkR"),0.f}};
    auto F=UVRMFaceLibrary::NormalizeCurves(Raw);
    TestEqual(TEXT("Automatic graph uses one actual node"),Set(Auto,F),1);
    TestTrue(TEXT("Modular graph exercises multiple actual nodes"),Set(Modular,F)>1);
    for(int32 Phase=0;Phase<2;++Phase)
    {
        if(Phase==1){for(auto& P:Raw)P.Value=0.f;F=UVRMFaceLibrary::NormalizeCurves(Raw);Set(Auto,F);Set(Modular,F);}
        for(int32 I=0;I<60;++I)for(auto* C:{Auto,Modular}){C->TickAnimation(1.f/60.f,false);C->RefreshBoneTransforms();C->CompleteParallelAnimationEvaluation(true);}
        for(FName N:UVRMFaceLibrary::SemanticNames())
            TestTrue(*FString::Printf(TEXT("Actual auto/modular curve %s phase %d"),*N.ToString(),Phase),FMath::IsNearlyEqual(Auto->GetAnimInstance()->GetCurveValue(FName(TEXT("VFN_")+N.ToString())),Modular->GetAnimInstance()->GetCurveValue(FName(TEXT("VFN_")+N.ToString())),.0001f));
        const float Jaw=Auto->GetAnimInstance()->GetCurveValue(TEXT("VFN_JawOpen"));
        if(Phase==0)
        {
            TestTrue(TEXT("Front camera blink preserves original manual side"),Auto->GetAnimInstance()->GetCurveValue(TEXT("VFN_EyeBlinkRight"))>1.09f);
            TestTrue(TEXT("Other eye is not closed by a second mirror"),Auto->GetAnimInstance()->GetCurveValue(TEXT("VFN_EyeBlinkLeft"))<.001f);
        }
        AddInfo(FString::Printf(TEXT("Actual animation phase %d: jaw %.6f, manual %.0f, connected %.0f"),Phase,Jaw,Auto->GetAnimInstance()->GetCurveValue(TEXT("VFN_Manual_JawOpen")),Auto->GetAnimInstance()->GetCurveValue(TEXT("VFN_Connected"))));
        TestTrue(*FString::Printf(TEXT("Actual node manual jaw phase %d value %.6f"),Phase,Jaw),Phase==0?FMath::IsNearlyEqual(Jaw,(.59f-.03f)/.97f,.001f):Jaw<.001f);
    }
    Auto->GetOwner()->Destroy();Modular->GetOwner()->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
