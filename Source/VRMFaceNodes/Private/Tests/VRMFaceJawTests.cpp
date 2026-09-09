#include "VRMFaceLibrary.h"
#include "VRMFaceCalibration.h"
#include "VRMFaceCalibrationGuide.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNThreePointJawTest,"VRMFaceNodes.ThreePointJaw",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNThreePointJawTest::RunTest(const FString&)
{
    TArray<float> Idle,Normal,Wide;Idle.Init(.04f,150);Normal.Init(.35f,150);Wide.Init(.8f,150);
    Idle[30]=.9f;Normal[30]=1.f;Wide[30]=1.f;FVFNCurveRange Jaw;FString Message;
    TestTrue(TEXT("Three stable levels fit despite isolated spikes"),UVFNCalibrationLibrary::FitJawRange(Idle,Normal,Wide,false,Jaw,Message));
    TestTrue(TEXT("Normal and maximum anchors reject isolated peaks"),FMath::IsNearlyEqual(Jaw.Typical,.35f)&&FMath::IsNearlyEqual(Jaw.Maximum,.8f));
    FVFNPerformerProfile P;P.SchemaVersion=4;P.NeutralSamples=150;P.Ranges.Add(TEXT("JawOpen"),Jaw);
    TestTrue(TEXT("New jaw profile validates"),UVFNCalibrationLibrary::ValidateProfile(P,Message));
    FVFNFrame F;F.bValid=true;F.Curves={{TEXT("JawOpen"),0.f},{TEXT("MouthRollUpper"),.7f}};
    auto Response=[&](float Value){F.Curves[TEXT("JawOpen")]=Value;return UVFNCalibrationLibrary::ApplyProfile(F,P).Curves.FindRef(TEXT("JawOpen"));};
    TestEqual(TEXT("Resting lips stay closed"),Response(.04f),0.f);
    TestTrue(TEXT("Small opening is not full opening"),Response(.09f)<.15f);
    TestTrue(TEXT("Normal speaking opens to forty percent"),FMath::IsNearlyEqual(Response(.35f),.4f,.00001f));
    TestTrue(TEXT("Movement above normal still leaves expressive room"),Response(.4f)<.6f);
    TestEqual(TEXT("Comfortable maximum remains reachable"),Response(.8f),1.f);
    float Previous=0.f;
    for(int32 I=0;I<=1000;++I)
    {const float Value=Response(I/1000.f);if(Value+1e-6f<Previous||Value<0.f||Value>1.f){AddError(TEXT("Jaw response must be monotone without overshoot"));break;}Previous=Value;}
    const float E=.0001f,L=(Response(.35f)-Response(.35f-E))/E,R=(Response(.35f+E)-Response(.35f))/E;
    TestTrue(TEXT("Slope is continuous around normal speech"),FMath::Abs(L-R)<.01f);
    TestEqual(TEXT("Unrelated mouth shapes retain their input"),UVFNCalibrationLibrary::ApplyProfile(F,P).Curves.FindRef(TEXT("MouthRollUpper")),.7f);
    TestTrue(TEXT("Jaw is marked calibrated to avoid another rest deadzone"),UVFNCalibrationLibrary::ApplyProfile(F,P).PerformerCalibratedChannels.Contains(TEXT("JawOpen")));
    FVFNCurveRange Rejected;
    TestFalse(TEXT("Identical ordinary and extreme recordings cannot fit"),UVFNCalibrationLibrary::FitJawRange(Idle,Normal,Normal,false,Rejected,Message));
    TArray<float> TinyNormal,TinyWide;TinyNormal.Init(.07f,150);TinyWide.Init(.10f,150);
    TestFalse(TEXT("Tiny signals cannot create a highly sensitive jaw"),UVFNCalibrationLibrary::FitJawRange(Idle,TinyNormal,TinyWide,false,Rejected,Message));
    TArray<float> Short;Short.Init(.8f,10);
    TestFalse(TEXT("An incomplete maximum cannot replace a jaw"),UVFNCalibrationLibrary::FitJawRange(Idle,Normal,Short,false,Rejected,Message));
    Wide.Init(.8f,150);Normal.Init(.5f,150);Idle.Init(.02f,150);
    TestTrue(TEXT("Extended MetaHuman jaw can calibrate"),UVFNCalibrationLibrary::FitJawRange(Idle,Normal,Wide,true,Jaw,Message));
    P.bMetaHuman=true;P.Ranges[TEXT("JawOpen")]=Jaw;
    TMap<FName,float> Raw={{TEXT("CTRL_expressions_jawOpen"),1.f},{TEXT("CTRL_expressions_jawOpenExtreme"),0.f}};
    F=UVRMFaceLibrary::NormalizeCurves(Raw);
    TestTrue(TEXT("MetaHuman auxiliary is preserved without changing the base channel"),F.bHasJawOpenExtreme&&F.Curves.FindRef(TEXT("JawOpen"))==1.f);
    TestTrue(TEXT("Saturated basic jaw can remain ordinary speech"),FMath::IsNearlyEqual(UVFNCalibrationLibrary::ApplyProfile(F,P).Curves.FindRef(TEXT("JawOpen")),.4f,.00001f));
    Raw[TEXT("CTRL_expressions_jawOpenExtreme")]=.6f;F=UVRMFaceLibrary::NormalizeCurves(Raw);
    TestEqual(TEXT("Extreme auxiliary reaches the calibrated maximum"),UVFNCalibrationLibrary::ApplyProfile(F,P).Curves.FindRef(TEXT("JawOpen")),1.f);
    Raw.Remove(TEXT("CTRL_expressions_jawOpenExtreme"));F=UVRMFaceLibrary::NormalizeCurves(Raw);
    TestTrue(TEXT("Missing auxiliary cannot turn ordinary speech into maximum"),FMath::IsNearlyEqual(UVFNCalibrationLibrary::ApplyProfile(F,P).Curves.FindRef(TEXT("JawOpen")),.4f,.00001f));
    Raw.Add(TEXT("CTRL_expressions_jawOpenExtreme"),.9f);Raw.Add(TEXT("JawOpen"),.2f);F=UVRMFaceLibrary::NormalizeCurves(Raw);
    TestTrue(TEXT("Direct ARKit jaw retains precedence and excludes MetaHuman extra"),!F.bHasJawOpenExtreme&&F.Curves.FindRef(TEXT("JawOpen"))==.2f);
    FVFNPerformerProfile Old;Old.SchemaVersion=3;Old.NeutralSamples=150;FVFNCurveRange Legacy;Legacy.RestCeiling=.1f;Legacy.Maximum=.9f;Legacy.bEnabled=true;Old.Ranges.Add(TEXT("JawOpen"),Legacy);Old.bMetaHuman=true;
    Raw.Remove(TEXT("JawOpen"));Raw[TEXT("CTRL_expressions_jawOpen")]=.5f;F=UVRMFaceLibrary::NormalizeCurves(Raw);
    const float LegacyWithExtra=UVFNCalibrationLibrary::ApplyProfile(F,Old).Curves.FindRef(TEXT("JawOpen"));
    Raw.Remove(TEXT("CTRL_expressions_jawOpenExtreme"));F=UVRMFaceLibrary::NormalizeCurves(Raw);
    TestEqual(TEXT("Schema three mapping ignores new auxiliary data"),LegacyWithExtra,UVFNCalibrationLibrary::ApplyProfile(F,Old).Curves.FindRef(TEXT("JawOpen")));
    TestTrue(TEXT("Schema three retains its original linear mapping"),FMath::IsNearlyEqual(LegacyWithExtra,.5f,.000001f));
    FString Json;TestTrue(TEXT("Three point profile exports JSON"),FJsonObjectConverter::UStructToJsonObjectString(P,Json));
    FVFNPerformerProfile Imported;TestTrue(TEXT("Three point JSON imports"),FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Imported,0,0));
    TestTrue(TEXT("JSON retains anchors and input representation"),UVFNCalibrationLibrary::ValidateProfile(Imported,Message)&&Imported.Ranges.FindChecked(TEXT("JawOpen")).bUseJawOpenExtreme&&Imported.Ranges.FindChecked(TEXT("JawOpen")).Typical==.5f);
    const FString Slot=TEXT("VFN_JawTest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto* Save=Cast<UVFNCalibrationSave>(UGameplayStatics::CreateSaveGameObject(UVFNCalibrationSave::StaticClass()));Save->Profile=P;
    TestTrue(TEXT("Three point profile saves"),UGameplayStatics::SaveGameToSlot(Save,Slot,0));
    if(auto* Loaded=Cast<UVFNCalibrationSave>(UGameplayStatics::LoadGameFromSlot(Slot,0)))TestTrue(TEXT("Binary round trip retains the new jaw mapping"),Loaded->Profile.SchemaVersion==4&&Loaded->Profile.Ranges.FindChecked(TEXT("JawOpen")).bThreePointJaw&&Loaded->Profile.Ranges.FindChecked(TEXT("JawOpen")).Typical==.5f);
    else AddError(TEXT("Three point binary did not load"));
    UGameplayStatics::DeleteGameInSlot(Slot,0);
    Imported.Ranges[TEXT("JawOpen")].Typical=Imported.Ranges[TEXT("JawOpen")].Maximum;
    TestFalse(TEXT("Malformed middle anchor is rejected"),UVFNCalibrationLibrary::ValidateProfile(Imported,Message));
    P.SchemaVersion=3;TestFalse(TEXT("New anchors cannot masquerade as an old profile"),UVFNCalibrationLibrary::ValidateProfile(P,Message));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNJawWizardTest,"VRMFaceNodes.JawCalibrationWizard",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNJawWizardTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().CreateAISystem(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* G=World->SpawnActor<AVRMFaceCalibrationGuide>();G->bPromptSound=false;G->bShowInterface=false;G->bAutomaticInput=false;
    FVFNPerformerProfile Before;Before.NeutralSamples=100;Before.bMetaHuman=true;Before.bRecenterHead=true;Before.NeutralHead=FRotator(5,6,7);Before.bMotionMeasured=true;Before.MotionStart=50;Before.MotionFull=150;Before.bOverrideMirror=true;Before.bMirrorCapture=true;
    FVFNCurveRange Old;Old.RestCeiling=.05f;Old.Maximum=.55f;Old.bEnabled=true;Old.SampleCount=100;
    for(FName N:{FName(TEXT("JawOpen")),FName(TEXT("EyeBlinkLeft")),FName(TEXT("BrowInnerUp")),FName(TEXT("MouthLowerDownLeft"))})Before.Ranges.Add(N,Old);
    G->Candidate=Before;double Stamp=1.;FVFNFrame Frame;Frame.bValid=true;Frame.bMetaHuman=true;Frame.bHasJawOpenExtreme=true;
    auto Feed=[&](bool SameLevels=false)
    {
        Frame.Curves={{TEXT("JawOpen"),G->StepIndex==0?.04f:1.f},{TEXT("EyeBlinkLeft"),.8f}};
        Frame.JawOpenExtreme=G->StepIndex==1&&G->JawPhase==1&&!SameLevels?.6f:0.f;
        Frame.SourceTimestamp=Stamp+=1./30.;G->FeedFrame(Frame,1.f/30.f);
    };
    G->StartJawCalibration();TArray<FString> Prompts;bool CheckedInterim=false;
    for(int32 I=0;I<1000&&G->bRunning;++I)
    {
        if(G->StepIndex==1&&G->JawPhase==1&&!CheckedInterim)
        {CheckedInterim=true;TestTrue(TEXT("Ordinary stage cannot commit a replacement by itself"),!G->Candidate.Ranges.FindChecked(TEXT("JawOpen")).bThreePointJaw);}
        Prompts.AddUnique(G->GetPrompt());Feed();
    }
    TestTrue(TEXT("Single jaw completes through three actual prompts"),G->bComplete&&Prompts.Num()==3&&CheckedInterim);
    TestTrue(TEXT("Single jaw stores three point MetaHuman input mode"),G->Candidate.SchemaVersion==4&&G->Candidate.Ranges.FindChecked(TEXT("JawOpen")).bThreePointJaw&&G->Candidate.Ranges.FindChecked(TEXT("JawOpen")).bUseJawOpenExtreme);
    for(FName N:{FName(TEXT("EyeBlinkLeft")),FName(TEXT("BrowInnerUp")),FName(TEXT("MouthLowerDownLeft"))})TestEqual(TEXT("Single jaw preserves unrelated ranges"),G->Candidate.Ranges.FindChecked(N).Maximum,Before.Ranges.FindChecked(N).Maximum);
    TestTrue(TEXT("Single jaw preserves head motion and mirror settings"),G->Candidate.NeutralHead.Equals(Before.NeutralHead)&&G->Candidate.MotionStart==50&&G->Candidate.MotionFull==150&&G->Candidate.bMirrorCapture);
    G->CancelCalibration();TestFalse(TEXT("Cancel restores the old jaw"),G->Candidate.Ranges.FindChecked(TEXT("JawOpen")).bThreePointJaw);
    G->StartJawCalibration();for(int32 I=0;I<1000&&G->bRunning;++I)Feed(true);
    TestTrue(TEXT("Indistinguishable ranges stop with original jaw intact"),!G->bComplete&&G->StepIndex==1&&!G->Candidate.Ranges.FindChecked(TEXT("JawOpen")).bThreePointJaw&&G->Status.Contains(TEXT("太接近")));
    G->RetryStep();TestEqual(TEXT("Retry restarts both jaw levels"),G->JawPhase,0);G->SkipStep();TestTrue(TEXT("Skip keeps the original jaw and finishes"),G->bComplete&&!G->Candidate.Ranges.FindChecked(TEXT("JawOpen")).bThreePointJaw);
    G->CancelCalibration();G->StartJawCalibration();
    for(int32 I=0;I<400&&G->StepIndex==0;++I)Feed();
    Frame.bHasJawOpenExtreme=false;for(int32 I=0;I<400&&G->bRunning;++I)Feed();
    TestTrue(TEXT("Input representation cannot change during capture"),!G->bComplete&&G->Status.Contains(TEXT("曲线类型改变"))&&!G->Candidate.Ranges.FindChecked(TEXT("JawOpen")).bThreePointJaw);
    G->CancelCalibration();G->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
