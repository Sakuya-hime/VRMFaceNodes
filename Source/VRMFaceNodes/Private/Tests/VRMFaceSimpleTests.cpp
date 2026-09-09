#include "Misc/AutomationTest.h"
#include "VRMFaceCalibration.h"
#include "VRMFaceCalibrationGuide.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "VRMFaceLibrary.h"
#include "AnimNode_VRMFace.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNSpeechCameraMathTest,"VRMFaceNodes.SimpleControlsMath",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNSpeechCameraMathTest::RunTest(const FString&)
{
    for(float Amount:{.5f,1.f,1.15f,1.5f})
    {
        float Previous=-1.f;
        for(int32 I=0;I<=1000;++I){const float V=UVFNCalibrationLibrary::SpeechResponse(I/1000.f,Amount);TestTrue(TEXT("Speech response is monotone and bounded"),V>=Previous && V>=0.f && V<=1.f);Previous=V;}
        TestEqual(TEXT("Closed mouth stays exactly closed"),UVFNCalibrationLibrary::SpeechResponse(0.f,Amount),0.f);
        TestEqual(TEXT("Maximum opening remains available"),UVFNCalibrationLibrary::SpeechResponse(1.f,Amount),1.f);
    }
    TestTrue(TEXT("Normal speech has more expression and still has headroom"),UVFNCalibrationLibrary::SpeechResponse(.4f,1.15f)>.4f && UVFNCalibrationLibrary::SpeechResponse(.4f,1.15f)<.5f);
    FVFNPerformerProfile P;P.SchemaVersion=4;P.NeutralSamples=100;P.bMetaHuman=true;
    FVFNCurveRange J;J.bEnabled=true;J.RestCeiling=.06f;J.Typical=.16f;J.Maximum=.45f;J.MaxGain=8.f;J.bThreePointJaw=true;J.bUseJawOpenExtreme=true;J.SampleCount=595;P.Ranges.Add(TEXT("JawOpen"),J);
    UVFNCalibrationLibrary::UpgradeProfile(P);
    TestTrue(TEXT("Upgrade uses personal jaw without replacing its input anchors"),P.SchemaVersion==5 && P.bUsePersonalJawWithManual && FVFNCurveRange::StaticStruct()->CompareScriptStruct(&J,&P.Ranges.FindChecked(TEXT("JawOpen")),PPF_None));
    P.bCameraCentered=true;P.bRecenterHead=true;P.bCameraVRoidAxes=true;P.NeutralHead=FRotator(22,-9,14);
    TestTrue(TEXT("Offset camera centers exactly"),UVFNCalibrationLibrary::RelativeCameraHead(P.NeutralHead,P).IsNearlyZero(.001f));
    const FRotator Base(-P.NeutralHead.Pitch,P.NeutralHead.Roll,P.NeutralHead.Yaw);
    const FRotator Wanted(8,12,-4),Rot=(Base.Quaternion()*Wanted.Quaternion()).Rotator();
    const FRotator Raw(-Rot.Pitch,Rot.Roll,Rot.Yaw),Expected(-Wanted.Pitch,Wanted.Roll,Wanted.Yaw);
    TestTrue(TEXT("Camera bias does not mix local nod, yaw and roll"),UVFNCalibrationLibrary::RelativeCameraHead(Raw,P).Equals(Expected,.001f));
    P.bCameraHeadRadians=true;P.NeutralHead*=PI/180.f;
    TestTrue(TEXT("Explicit radian mode follows the same orientation"),UVFNCalibrationLibrary::RelativeCameraHead(Raw*(PI/180.f),P).Equals(Expected*(PI/180.f),.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNCameraButtonTest,"VRMFaceNodes.CameraButtonAndChinRollback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNCameraButtonTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().CreateAISystem(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* G=World->SpawnActor<AVRMFaceCalibrationGuide>();G->bPromptSound=false;G->bShowInterface=false;G->bAutomaticInput=false;
    G->CenterCamera();TestFalse(TEXT("Button rejects missing live head data"),G->bCenteringCamera);
    FVFNFrame F;F.bValid=true;F.bHasHead=true;F.Head=FRotator(12,5,-8);F.SourceTimestamp=1.;
    F.Curves={{TEXT("JawOpen"),.08f},{TEXT("EyeLookOutLeft"),.3f},{TEXT("EyeLookInLeft"),.1f},{TEXT("EyeBlinkLeft"),.2f}};
    G->FeedFrame(F,1.f/60.f);G->CenterCamera();TestTrue(TEXT("One click starts a bounded camera sample"),G->bCenteringCamera);
    for(int32 I=0;I<60;++I){F.SourceTimestamp+=1.f/60.f;G->FeedFrame(F,1.f/60.f);}
    TestTrue(TEXT("Fresh stable input completes camera correction"),!G->bCenteringCamera && G->Candidate.bCameraCentered);
    auto Out=UVFNCalibrationLibrary::ApplyProfile(F,G->Candidate);
    TestTrue(TEXT("Avatar head and recorded gaze are centered"),Out.Head.IsNearlyZero(.001f) && FMath::IsNearlyZero(Out.Curves.FindRef(TEXT("EyeLookOutLeft")),.001f));
    TestEqual(TEXT("Camera correction does not recalibrate the mouth"),Out.Curves.FindRef(TEXT("JawOpen")),.08f);
    TestEqual(TEXT("Camera correction does not alter blinking"),Out.Curves.FindRef(TEXT("EyeBlinkLeft")),.2f);
    const auto Before=G->Candidate;G->CenterCamera();
    for(int32 I=0;I<270;++I)G->FeedFrame(F,1.f/60.f);
    TestTrue(TEXT("A paused source times out without replacing the prior camera center"),!G->bCenteringCamera && G->Candidate.NeutralHead.Equals(Before.NeutralHead));
    G->RestoreOriginalChin();TestEqual(TEXT("One click removes only chin assistance"),G->Candidate.ChinStrength,0.f);
    TestTrue(TEXT("Chin rollback preserves mouth, camera and enhancement mode"),G->Candidate.SpeechAmount==Before.SpeechAmount && G->Candidate.NeutralHead.Equals(Before.NeutralHead) && G->Candidate.bEnhancementsEnabled==Before.bEnhancementsEnabled);
    G->SetEnhancementsEnabled(false);TestFalse(TEXT("Master switch records the off mode"),G->Candidate.bEnhancementsEnabled);
    G->SetEnhancementsEnabled(true);TestTrue(TEXT("Turning on retains the explicitly restored chin"),G->Candidate.bEnhancementsEnabled && G->Candidate.ChinStrength==0.f);
    G->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNSimpleFlowTest,"VRMFaceNodes.SimpleFiveStepAutosave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNSimpleFlowTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().CreateAISystem(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* GI=NewObject<UGameInstance>(GEngine);World->SetGameInstance(GI);GI->Init();
    auto* G=World->SpawnActor<AVRMFaceCalibrationGuide>();G->bPromptSound=false;G->bShowInterface=false;G->bAutomaticInput=false;
    const FName Slot(*FString::Printf(TEXT("VFN130Test_%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    G->ProfileSlot=Slot;G->StartSimpleCalibration();
    TestTrue(TEXT("One button starts the five-step display"),G->GetPrompt().Contains(TEXT("1 / 5")));
    FVFNFrame F;F.bValid=true;F.bHasHead=true;F.SourceTimestamp=1.;
    TSet<FString> Prompts;
    for(int32 I=0;I<1800&&G->bRunning;++I)
    {
        Prompts.Add(G->GetPrompt());F.Curves.Empty();for(FName N:UVRMFaceLibrary::SemanticNames())F.Curves.Add(N,.02f);
        if(G->StepIndex==2)F.Curves[TEXT("EyeBlinkLeft")]=.8f; // Missing right action must not block the wizard.
        if(G->StepIndex==1)F.Curves[TEXT("JawOpen")]=G->JawPhase==0?.3f:.8f;
        if(G->StepIndex==4)for(FName N:{FName(TEXT("BrowInnerUp")),FName(TEXT("BrowOuterUpLeft")),FName(TEXT("BrowOuterUpRight"))})F.Curves[N]=.65f;
        F.SourceTimestamp+=1./30.;G->FeedFrame(F,1.f/30.f);
    }
    TestTrue(TEXT("Five steps complete and automatically save"),G->bComplete&&!G->bRunning&&G->Status.Contains(TEXT("已应用并保存")));
    TestEqual(TEXT("Ordinary and maximum mouth prompts are distinct"),Prompts.Num(),5);
    auto* S=GI->GetSubsystem<UVFNCalibrationSubsystem>();FString M;
    TestTrue(TEXT("Saved result can be reloaded from disk"),S->Reload(Slot,M));
    const auto* P=S->FindProfile(Slot);
    if(TestNotNull(TEXT("Autosaved profile exists"),P))
    {
        const auto* J=P->Ranges.Find(TEXT("JawOpen"));
        TestTrue(TEXT("Autosave retains distinct ordinary and maximum jaw anchors"),J&&J->bThreePointJaw&&J->Maximum>J->Typical+.3f);
        TestTrue(TEXT("Personal jaw is active after automatic completion"),P->SchemaVersion==5&&P->bUsePersonalJawWithManual);
    }
    G->RestoreOriginalChin();G->Tick(1.f);
    S->Reload(Slot,M);P=S->FindProfile(Slot);
    TestTrue(TEXT("Chin rollback automatically persists without a save button"),P&&P->ChinStrength==0.f);
    G->Destroy();S->DeleteProfile(Slot,M);GI->Shutdown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNChinAnimationTest,"VRMFaceNodes.ChinAndMasterAnimation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNChinAnimationTest::RunTest(const FString&)
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/VRMFaceNodesDemo/Comparison130/Enhanced/SK_FaceReady.SK_FaceReady"),nullptr,LOAD_NoWarn);
    auto* Class=LoadClass<UAnimInstance>(nullptr,TEXT("/Game/VRMFaceNodesDemo/Comparison130/Enhanced/ABP_AutoFace.ABP_AutoFace_C"),nullptr,LOAD_NoWarn);
    if(!Mesh||!Class){AddInfo(TEXT("Project-only chin geometry fixture unavailable in portable plugin."));return true;}
    const auto Init=UWorld::InitializationValues().CreateAISystem(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Actor=World->SpawnActor<AActor>();auto* C=NewObject<USkeletalMeshComponent>(Actor);Actor->AddInstanceComponent(C);Actor->SetRootComponent(C);
    C->SetDisablePostProcessBlueprint(true);C->SetSkeletalMeshAsset(Mesh);C->SetAnimInstanceClass(Class);C->RegisterComponent();
    auto* A=C->GetAnimInstance();FAnimNode_VRMFace* Node=nullptr;
    if(A)for(TFieldIterator<FStructProperty> It(A->GetClass());It;++It)if(It->Struct==FAnimNode_VRMFace::StaticStruct())Node=It->ContainerPtrToValuePtr<FAnimNode_VRMFace>(A);
    if(TestNotNull(TEXT("Enhanced comparison uses actual automatic node"),Node))
    {
        Node->Settings=FVFNSettings();Node->Settings.SpeechAmount=1.15f;Node->PerformerProfileSlot=NAME_None;Node->bUseTestFrame=true;
        auto SetJaw=[&](float Jaw){Node->TestFrame=UVRMFaceLibrary::NormalizeCurves({{TEXT("JawOpen"),Jaw},{TEXT("MouthLeft"),.8f},{TEXT("MouthRight"),0.f}});};
        auto Step=[&](){C->TickAnimation(1.f/60.f,false);C->RefreshBoneTransforms();C->CompleteParallelAnimationEvaluation(true);};
        SetJaw(.5f);for(int32 I=0;I<60;++I)Step();
        const float Jaw=A->GetCurveValue(TEXT("jawOpen")),Chin=A->GetCurveValue(TEXT("VFN_ChinAssist"));
        TestTrue(TEXT("New chin curve follows final jaw weight"),Chin>.01f&&FMath::IsNearlyEqual(Chin,Jaw*.35f,.0001f));
        Node->Settings.ChinStrength=0.f;Step();
        TestEqual(TEXT("Rollback removes chin in the next evaluation"),A->GetCurveValue(TEXT("VFN_ChinAssist")),0.f);
        TestTrue(TEXT("Rollback retains mouth opening"),FMath::IsNearlyEqual(A->GetCurveValue(TEXT("jawOpen")),Jaw,.0001f));
        Node->Settings.ChinStrength=.35f;Node->Settings.bEnhancementsEnabled=false;Step();
        TestEqual(TEXT("Master off removes chin"),A->GetCurveValue(TEXT("VFN_ChinAssist")),0.f);
        TestEqual(TEXT("Master off removes teeth correction"),A->GetCurveValue(TEXT("VRoid_TeethRetract")),0.f);
        TestTrue(TEXT("Master off retains direct jaw"),FMath::IsNearlyEqual(A->GetCurveValue(TEXT("jawOpen")),.5f,.0001f));
        Node->Settings.bEnhancementsEnabled=true;SetJaw(0.f);for(int32 I=0;I<60;++I)Step();
        TestTrue(TEXT("Closed mouth has no residual chin motion"),FMath::Abs(A->GetCurveValue(TEXT("VFN_ChinAssist")))<.0001f);
    }
    Actor->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
