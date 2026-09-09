#include "VRMFaceLibrary.h"
#include "VRMFaceCalibration.h"
#include "VRMFaceCalibrationGuide.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNMappingTest,"VRMFaceNodes.Mapping",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNMappingTest::RunTest(const FString&)
{
    TestEqual(TEXT("52 semantic channels"),UVRMFaceLibrary::SemanticNames().Num(),52);
    TMap<FName,float> Raw;
    Raw.Add(TEXT("CTRL_expressions_eyeBlinkL"),.6f);Raw.Add(TEXT("CTRL_expressions_mouthLipsPurseUL"),.8f);
    Raw.Add(TEXT("CTRL_expressions_mouthUpperLipRollInL"),.6f);Raw.Add(TEXT("CTRL_expressions_mouthRight"),1.f);
    Raw.Add(TEXT("HeadYaw"),10.f);
    FVFNFrame F=UVRMFaceLibrary::NormalizeCurves(Raw);
    TestTrue(TEXT("MetaHuman detected"),F.bMetaHuman && F.bHasHead);
    TestEqual(TEXT("Strongest pucker quadrant"),F.Curves.FindRef(TEXT("MouthPucker")),.8f);
    TestEqual(TEXT("No sideways clamp"),F.Curves.FindRef(TEXT("MouthRight")),1.f);
    Raw.Add(TEXT("EyeBlinkLeft"),.2f);F=UVRMFaceLibrary::NormalizeCurves(Raw);
    TestEqual(TEXT("Direct ARKit wins"),F.Curves.FindRef(TEXT("EyeBlinkLeft")),.2f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNCalibrationTest,"VRMFaceNodes.Calibration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNCalibrationTest::RunTest(const FString&)
{
    FVFNSettings S;S.bMirrorCapture=false;S.bSwapEyes=false;
    FVFNFrame F; F.bValid=true;
    F.Curves={{TEXT("EyeBlinkLeft"),.6f},{TEXT("EyeBlinkRight"),0.f},{TEXT("MouthPucker"),1.f},{TEXT("MouthClose"),1.f},{TEXT("JawOpen"),.9f},{TEXT("MouthRollUpper"),.6f},{TEXT("MouthRight"),1.f}};
    FVFNState State; FVFNFrame R;
    for(int32 I=0;I<120;++I) R=UVRMFaceLibrary::Calibrate(F,S,State,1.f/60.f);
    TestTrue(TEXT("Blink reaches 1.1"),FMath::IsNearlyEqual(R.Curves.FindRef(TEXT("EyeBlinkLeft")),1.1f,.0001f));
    TestEqual(TEXT("Independent right eye"),R.Curves.FindRef(TEXT("EyeBlinkRight")),0.f);
    TestTrue(TEXT("Pucker .4 -> .352"),FMath::IsNearlyEqual(R.Curves.FindRef(TEXT("MouthPucker")),.352f,.0001f));
    TestEqual(TEXT("No excessive mouth close"),R.Curves.FindRef(TEXT("MouthClose")),0.f);
    TestEqual(TEXT("Lip roll limited"),R.Curves.FindRef(TEXT("MouthRollUpper")),.1f);
    TestEqual(TEXT("Sideways preserved"),R.Curves.FindRef(TEXT("MouthRight")),1.f);
    FVFNState Other;auto Z=UVRMFaceLibrary::Calibrate(F,S,Other,1.f/60.f);
    TestTrue(TEXT("State belongs to instance"),Z.Curves.FindRef(TEXT("EyeBlinkLeft"))<1.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNStabilityTest,"VRMFaceNodes.Stability",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNStabilityTest::RunTest(const FString&)
{
    FVFNSettings S;S.bMirrorCapture=false; FVFNState State; FVFNFrame F; F.bValid=true;F.bHasHead=true;
    F.Curves={{TEXT("MouthRight"),.8f},{TEXT("BrowInnerUp"),.8f},{TEXT("EyeBlinkLeft"),0.f}};
    auto Step=[&](int32 Count,float Speed){FVFNFrame R;for(int32 I=0;I<Count;++I){F.Head.Yaw+=Speed/60.f;R=UVRMFaceLibrary::Stabilize(F,S,State,1.f/60.f);}return R;};
    Step(60,0);Step(18,300);F.Curves[TEXT("MouthRight")]=.2f;F.Curves[TEXT("BrowInnerUp")]=.2f;F.Curves[TEXT("EyeBlinkLeft")]=.35f;
    auto R=Step(1,300);
    TestTrue(TEXT("First-frame mouth change remains smoothed"),R.Curves[TEXT("MouthRight")]>.4f);
    TestEqual(TEXT("First-frame blink spike rejected"),R.Curves[TEXT("EyeBlinkLeft")],0.f);
    R=Step(7,300);
    TestTrue(TEXT("Sustained mouth action responds during motion"),R.Curves[TEXT("MouthRight")]<.24f);
    TestTrue(TEXT("Partial blink need not reach full amplitude to pass"),R.Curves[TEXT("EyeBlinkLeft")]>.33f);
    F.Curves[TEXT("EyeBlinkLeft")]=1.1f;R=Step(10,300);
    TestTrue(TEXT("Deliberate blink still fast"),R.Curves[TEXT("EyeBlinkLeft")]>1.08f);
    S.bStabilize=false;R=Step(1,300);
    TestEqual(TEXT("Disable passes through"),R.Curves[TEXT("MouthRight")],.2f);
    S.bStabilize=true;State.bHeadInitialized=true;State.PreviousHead=FRotator(0,179,0);F.Head=FRotator(0,-179,0);State.MotionProtection=0;
    UVRMFaceLibrary::Stabilize(F,S,State,.1f);TestEqual(TEXT("Angle wrapping does not spike"),State.MotionProtection,0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNMotionLatencyTest,"VRMFaceNodes.MotionExpressionLatency",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNMotionLatencyTest::RunTest(const FString&)
{
    for(int32 FPS:{30,60,120}) for(bool Mirror:{false,true})
    {
        FVFNSettings S;S.bMirrorCapture=Mirror;S.bSwapEyes=false;S.bCalibration=false;
        S.MotionStart=43.6f;S.MotionFull=158.45f;
        FVFNState State,Calibration;FVFNFrame F;F.bValid=true;F.bHasHead=true;
        F.Curves={{TEXT("BrowInnerUp"),.7f},{TEXT("EyeBlinkLeft"),0.f},
            {TEXT("EyeBlinkRight"),1.f},{TEXT("EyeSquintLeft"),.1f},{TEXT("JawOpen"),0.f},{TEXT("EyeLookUpLeft"),.8f}};
        const FName OpenEye=Mirror?TEXT("EyeBlinkRight"):TEXT("EyeBlinkLeft");
        const FName ClosedEye=Mirror?TEXT("EyeBlinkLeft"):TEXT("EyeBlinkRight");
        const FName Lid=Mirror?TEXT("EyeSquintRight"):TEXT("EyeSquintLeft");
        const FName Gaze=Mirror?TEXT("EyeLookUpRight"):TEXT("EyeLookUpLeft");
        const float Dt=1.f/FPS;float Time=0.f;FVFNFrame R;
        auto Step=[&](int32 Count,bool Moving)
        {
            for(int32 I=0;I<Count;++I)
            {
                Time+=Dt;
                if(Moving)F.Head=FRotator(20.f*FMath::Sin(Time*16.f),16.f*FMath::Cos(Time*16.f),0);
                R=UVRMFaceLibrary::Calibrate(F,S,Calibration,Dt);
                R=UVRMFaceLibrary::Stabilize(R,S,State,Dt);
            }
        };
        Step(FPS,false);Step(FPS,true);
        F.Curves[TEXT("BrowInnerUp")]=.1f;F.Curves[TEXT("EyeBlinkLeft")]=1.f;
        F.Curves[TEXT("EyeBlinkRight")]=.2f;F.Curves[TEXT("EyeSquintLeft")]=.8f;
        Step(1,true);
        TestEqual(TEXT("Isolated false closure rejected"),R.Curves.FindRef(OpenEye),0.f);
        TestEqual(TEXT("Isolated closed-eye weight loss rejected"),R.Curves.FindRef(ClosedEye),1.f);
        TestTrue(TEXT("Isolated brow spike rejected"),FMath::IsNearlyEqual(R.Curves.FindRef(TEXT("BrowInnerUp")),.7f));
        F.Curves[TEXT("BrowInnerUp")]=.7f;F.Curves[TEXT("EyeBlinkLeft")]=0.f;
        F.Curves[TEXT("EyeBlinkRight")]=1.f;F.Curves[TEXT("EyeSquintLeft")]=.1f;
        Step(2,true);
        TestEqual(TEXT("Rejected spike is not replayed on a later frame"),R.Curves.FindRef(OpenEye),0.f);
        TestTrue(TEXT("Rejected brow spike does not echo"),FMath::IsNearlyEqual(R.Curves.FindRef(TEXT("BrowInnerUp")),.7f));
        // Partial expressions must respond while the head CONTINUES moving.
        F.Curves[TEXT("BrowInnerUp")]=.15f;F.Curves[TEXT("EyeBlinkLeft")]=.55f;
        F.Curves[TEXT("EyeBlinkRight")]=.45f;F.Curves[TEXT("EyeSquintLeft")]=.65f;F.Curves[TEXT("JawOpen")]=.8f;
        const TMap<FName,float> From={{OpenEye,0.f},{ClosedEye,1.f},{TEXT("BrowInnerUp"),.7f},{Lid,.1f},{TEXT("JawOpen"),0.f}};
        const TMap<FName,float> To={{OpenEye,.55f},{ClosedEye,.45f},{TEXT("BrowInnerUp"),.15f},{Lid,.65f},{TEXT("JawOpen"),.8f}};
        TMap<FName,float> Latency,Onset;
        for(int32 I=0;I<FPS/2;++I)
        {
            Step(1,true);
            for(const auto& P:To)
            {
                const float Fraction=(R.Curves.FindRef(P.Key)-From.FindRef(P.Key))/(P.Value-From.FindRef(P.Key));
                if(Fraction>.1f && !Onset.Contains(P.Key))Onset.Add(P.Key,(I+1)*Dt);
                if(Fraction>=.9f && !Latency.Contains(P.Key))Latency.Add(P.Key,(I+1)*Dt);
            }
        }
        for(const auto& P:To)
        {
            TestTrue(TEXT("Expression starts within two animation ticks"),Onset.Contains(P.Key) && Onset.FindRef(P.Key)<=2.f*Dt+.001f);
            TestTrue(TEXT("90 percent response within 140 ms while shaking"),Latency.Contains(P.Key) && Latency.FindRef(P.Key)<=.14f);
            AddInfo(FString::Printf(TEXT("%d FPS mirror=%d %s: t90=%.1f ms"),FPS,Mirror,*P.Key.ToString(),Latency.FindRef(P.Key)*1000.f));
        }
        TestEqual(TEXT("Gaze does not enter the expression filter"),R.Curves.FindRef(Gaze),.8f);
        F.Curves[TEXT("EyeBlinkLeft")]=1.f;Step(FMath::CeilToInt(.1f*FPS),true);
        TestTrue(TEXT("Deliberate close follows during motion"),R.Curves.FindRef(OpenEye)>.97f);
        F.Curves[TEXT("EyeBlinkLeft")]=0.f;Step(FMath::CeilToInt(.1f*FPS),true);
        TestTrue(TEXT("Reopening is not held closed"),R.Curves.FindRef(OpenEye)<.04f);
        // Continuous, sub-maximal brow movement must not staircase behind an anchor.
        F.Curves[TEXT("BrowInnerUp")]=0.f;Step(FPS/2,true);float MaxLag=0.f;
        for(int32 I=1;I<=FPS;++I){F.Curves[TEXT("BrowInnerUp")]=I*Dt;Step(1,true);MaxLag=FMath::Max(MaxLag,I*Dt-R.Curves.FindRef(TEXT("BrowInnerUp")));}
        TestTrue(TEXT("Continuous brow ramp has bounded lag"),MaxLag<.12f);
        Step(FPS,false);TestFalse(TEXT("Filter releases when stationary"),State.bMotionFiltering);
        S.bStabilize=false;F.Curves[TEXT("BrowInnerUp")]=.3f;Step(1,true);
        TestEqual(TEXT("Disable passes through"),R.Curves.FindRef(TEXT("BrowInnerUp")),.3f);
        TestTrue(TEXT("Disable clears temporal samples"),State.MotionInputPrevious.IsEmpty() && State.MotionInputOlder.IsEmpty());
        S.bStabilize=true;Step(4,true);F.bValid=false;Step(1,true);
        TestFalse(TEXT("Lost capture releases filter"),State.bMotionFiltering);
        TestTrue(TEXT("Lost capture clears stale samples"),State.MotionInputPrevious.IsEmpty());
        F.bValid=true;S.bHoldMotionExpressions=false;Step(4,true);
        TestFalse(TEXT("Spike filter can be independently disabled"),State.bMotionFiltering);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNCalibratedBlinkLatencyTest,"VRMFaceNodes.CalibratedBlinkLatency",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNCalibratedBlinkLatencyTest::RunTest(const FString&)
{
    for(int32 FPS:{30,60,120})
    {
        FVFNSettings S;S.bMirrorCapture=false;S.bSwapEyes=false;S.MotionStart=43.6f;S.MotionFull=158.45f;
        FVFNFrame F;F.bValid=true;F.bHasHead=true;F.Curves.Add(TEXT("EyeBlinkLeft"),0.f);
        FVFNState Calibration,Stability;const float Dt=1.f/FPS;
        auto Step=[&](){F.Head.Yaw+=300.f*Dt;auto R=UVRMFaceLibrary::Calibrate(F,S,Calibration,Dt);return UVRMFaceLibrary::Stabilize(R,S,Stability,Dt).Curves.FindRef(TEXT("EyeBlinkLeft"));};
        for(int32 I=0;I<FPS;++I)Step();
        for(bool Closing:{true,false})
        {
            F.Curves[TEXT("EyeBlinkLeft")]=Closing?S.BlinkEnd:0.f;float Reached=-1.f;
            for(int32 I=0;I<FPS/2;++I){const float V=Step();if(Reached<0.f && (Closing?V>=.9f*S.BlinkGain:V<=.1f*S.BlinkGain))Reached=(I+1)*Dt;}
            TestTrue(TEXT("Calibration plus stabilization stays responsive"),Reached>0.f && Reached<=.20f);
            AddInfo(FString::Printf(TEXT("Calibrated blink %d FPS closing=%d: t90=%.1f ms"),FPS,Closing,Reached*1000.f));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNTimeTest,"VRMFaceNodes.TimeAndReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNTimeTest::RunTest(const FString&)
{
    float Reference=0.f;
    for (int32 FPS:{30,60,120})
    {
        float V=0.f;for(int32 I=0;I<FPS;++I)V=FMath::Lerp(V,1.f,UVRMFaceLibrary::TimeAlpha(.2f,1.f/FPS));
        if(FPS==30)Reference=V;else TestTrue(TEXT("Frame-rate independent response"),FMath::IsNearlyEqual(V,Reference,.00001f));
    }
    TestEqual(TEXT("Long pause resets stale interpolation"),UVRMFaceLibrary::TimeAlpha(.2f,1.f),1.f);
    TestEqual(TEXT("Zero delta is bounded"),UVRMFaceLibrary::TimeAlpha(.2f,0.f),1.f);
    FVFNSettings S;S.bMirrorCapture=false; FVFNState State; FVFNFrame F;F.Curves.Add(TEXT("MouthRight"),1.f);
    auto R=UVRMFaceLibrary::RetractTeeth(F,S,State,1.f);TestEqual(TEXT("Full teeth correction"),R.Curves.FindRef(TEXT("VRoid_TeethRetract")),.6f);
    UVRMFaceLibrary::ResetState(State);TestEqual(TEXT("Reset clears teeth history"),State.TeethWeight,0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNEyesAndPresetTest,"VRMFaceNodes.EyesAndPreset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNEyesAndPresetTest::RunTest(const FString&)
{
    FVFNSettings S;S.bMirrorCapture=false;S.bCalibration=false;FVFNState State;FVFNFrame F;
    F.Curves={{TEXT("EyeBlinkLeft"),.8f},{TEXT("EyeBlinkRight"),.2f},
        {TEXT("EyeLookInLeft"),0.f},{TEXT("EyeLookOutLeft"),.6f},
        {TEXT("EyeLookInRight"),.6f},{TEXT("EyeLookOutRight"),0.f},
        {TEXT("EyeLookUpLeft"),1.f},{TEXT("EyeLookUpRight"),1.f}};
    const auto R=UVRMFaceLibrary::Calibrate(F,S,State,1.f/60.f);
    TestEqual(TEXT("Blink sides swapped"),R.Curves.FindRef(TEXT("EyeBlinkRight")),.8f);
    TestEqual(TEXT("World-relative gaze kept on left"),R.Curves.FindRef(TEXT("EyeLookOutLeft")),.6f);
    TestEqual(TEXT("World-relative gaze kept on right"),R.Curves.FindRef(TEXT("EyeLookInRight")),.6f);
    FVFNModelProfile Generic;const auto Neutral=UVRMFaceLibrary::ResolveSettings(FVFNSettings(),Generic);
    TestFalse(TEXT("Unknown model does not receive VRoid axes"),Neutral.bVRoidAxes);
    TestFalse(TEXT("Unknown model does not receive VRoid mouth calibration"),Neutral.bCalibration);
    F.Head=FRotator(10.,15.,20.);F.bHasHead=true;S.bCalibration=true;
    UVRMFaceLibrary::FollowHead(F,S,State,1.f);
    TestTrue(TEXT("Legacy VRoid head axes retained"),State.SmoothedHead.Equals(FRotator(-8.,16.,15.),.001f));
    TestTrue(TEXT("Seated body gain retained"),State.SmoothedBody.Equals(State.SmoothedHead*.32f,.001f));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNPersonalCalibrationTest,"VRMFaceNodes.PersonalCalibration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNPersonalCalibrationTest::RunTest(const FString&)
{
    TArray<float> Idle,Open;
    for(int32 I=0;I<100;++I) {Idle.Add(.075f+(I%5)*.002f);Open.Add(.76f+(I%8)*.004f);}
    Idle[99]=.6f;Open[99]=1.f; // A single mistracked frame must not set the range.
    FVFNCurveRange Range;
    TestTrue(TEXT("Stable observations fit"),UVFNCalibrationLibrary::FitRange(Idle,Open,Range));
    TestTrue(TEXT("Neutral outlier rejected"),Range.RestCeiling<.1f);
    TestTrue(TEXT("Peak outlier rejected"),Range.Maximum<.81f);
    FVFNPerformerProfile P;P.NeutralSamples=100;P.Ranges.Add(TEXT("JawOpen"),Range);P.bRecenterHead=true;P.NeutralHead=FRotator(0,179,0);
    FVFNFrame F;F.bValid=true;F.bHasHead=true;F.Head=FRotator(0,-179,0);F.Curves={{TEXT("JawOpen"),.08f},{TEXT("MouthRight"),.7f}};
    auto R=UVFNCalibrationLibrary::ApplyProfile(F,P);
    TestEqual(TEXT("Natural closed mouth removes residual jaw input"),R.Curves[TEXT("JawOpen")],0.f);
    TestEqual(TEXT("Uncalibrated channel is unchanged"),R.Curves[TEXT("MouthRight")],.7f);
    TestTrue(TEXT("Head recenters across wrap"),FMath::IsNearlyEqual(R.Head.Yaw,2.,.001));
    F.Curves[TEXT("JawOpen")]=Range.Maximum;R=UVFNCalibrationLibrary::ApplyProfile(F,P);
    TestTrue(TEXT("Comfortable open amplitude remains reachable"),R.Curves[TEXT("JawOpen")]>.99f);
    F.bMetaHuman=true;R=UVFNCalibrationLibrary::ApplyProfile(F,P);
    TestEqual(TEXT("Different source representation is not recalibrated"),R.Curves[TEXT("JawOpen")],Range.Maximum);
    FString M;TestTrue(TEXT("Valid profile accepted"),UVFNCalibrationLibrary::ValidateProfile(P,M));
    P.Ranges[TEXT("JawOpen")].Maximum=P.Ranges[TEXT("JawOpen")].RestCeiling;
    TestFalse(TEXT("Degenerate range rejected"),UVFNCalibrationLibrary::ValidateProfile(P,M));
    TestFalse(TEXT("Profile slot cannot escape SaveGames"),UVFNCalibrationLibrary::IsValidSlot(TEXT("../other")));
    TestTrue(TEXT("Chinese profile name supported"),UVFNCalibrationLibrary::IsValidSlot(TEXT("坐播校准")));
    FVFNSettings S;S.bMirrorCapture=false;S.bSwapEyes=false;S.JawRestThreshold=.1f;FVFNState State;
    F.bMetaHuman=false;F.Curves[TEXT("JawOpen")]=.08f;R=UVRMFaceLibrary::Calibrate(F,S,State,1.f/60.f);
    TestEqual(TEXT("Optional uncalibrated jaw dead zone closes residual"),R.Curves[TEXT("JawOpen")],0.f);
    F.Curves[TEXT("JawOpen")]=.5f;F.PerformerCalibratedChannels.Add(TEXT("JawOpen"));R=UVRMFaceLibrary::Calibrate(F,S,State,1.f/60.f);
    TestEqual(TEXT("Personal calibration is not applied twice"),R.Curves[TEXT("JawOpen")],.5f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNRobustRangeTest,"VRMFaceNodes.CalibrationRobustRanges",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNRobustRangeTest::RunTest(const FString&)
{
    TArray<float> Idle,Action;
    for(int32 I=0;I<300;++I){Idle.Add(I%60<8?.9f:.35f+(I%3-1)*.001f);Action.Add(.415f+(I%3-1)*.001f);}
    FVFNCurveRange R;TestTrue(TEXT("Normal blink outliers do not reject small reliable actions"),UVFNCalibrationLibrary::FitRange(Idle,Action,R));
    TestTrue(TEXT("Blink outliers do not contaminate neutral baseline"),FMath::Abs(R.Neutral-.35f)<.003f && R.RestCeiling<.36f);
    FVFNPerformerProfile P;P.SchemaVersion=3;P.NeutralSamples=300;P.Ranges.Add(TEXT("EyeWideLeft"),R);FString M;
    TestTrue(TEXT("Small range passes save/load validation"),UVFNCalibrationLibrary::ValidateProfile(P,M));
    FVFNFrame F;F.bValid=true;F.Curves.Add(TEXT("EyeWideLeft"),R.Maximum);
    TestTrue(TEXT("Measured small maximum is reachable"),UVFNCalibrationLibrary::ApplyProfile(F,P).Curves.FindRef(TEXT("EyeWideLeft"))>.99f);
    F.Curves[TEXT("EyeWideLeft")]=R.Neutral;
    TestEqual(TEXT("High natural weight becomes neutral"),UVFNCalibrationLibrary::ApplyProfile(F,P).Curves.FindRef(TEXT("EyeWideLeft")),0.f);
    TArray<float> Flat;Flat.Init(.35f,300);TestFalse(TEXT("A flat source cannot fabricate calibration"),UVFNCalibrationLibrary::FitRange(Idle,Flat,R));
    Flat[15]=1.f;TestFalse(TEXT("One action spike cannot create a peak"),UVFNCalibrationLibrary::FitRange(Idle,Flat,R));
    TArray<float> Noise;for(int32 I=0;I<300;++I)Noise.Add(.1f+(I%30)*.02f);
    TestFalse(TEXT("Broad neutral noise is not mistaken for weak intentional movement"),UVFNCalibrationLibrary::FitRange(Noise,Action,R));
    TArray<float> Sparse;Sparse.Init(std::numeric_limits<float>::quiet_NaN(),100);Sparse[0]=.1f;
    TestFalse(TEXT("Insufficient finite samples rejected"),UVFNCalibrationLibrary::FitRange(Sparse,Action,R));
    FVFNCurveRange Legacy;Legacy.bEnabled=true;Legacy.Neutral=.01f;Legacy.RestCeiling=.02f;Legacy.Maximum=.17f;
    P={};P.SchemaVersion=2;P.NeutralSamples=300;P.Ranges.Add(TEXT("EyeWideLeft"),Legacy);F.Curves[TEXT("EyeWideLeft")]=.17f;
    TestTrue(TEXT("Legacy profile still validates"),UVFNCalibrationLibrary::ValidateProfile(P,M));
    TestTrue(TEXT("Legacy fourfold response is byte-compatible in behavior"),FMath::IsNearlyEqual(UVFNCalibrationLibrary::ApplyProfile(F,P).Curves.FindRef(TEXT("EyeWideLeft")),.6f));
    P.SchemaVersion=3;P.Ranges[TEXT("EyeWideLeft")].MaxGain=100.f;TestFalse(TEXT("Unsafe gain is not imported"),UVFNCalibrationLibrary::ValidateProfile(P,M));
    auto* LegacySave=NewObject<UVFNCalibrationSave>();
    TestEqual(TEXT("Omitted version fields retain legacy serialized default"),LegacySave->Profile.SchemaVersion,2);
    LegacySave->Profile.NeutralSamples=300;LegacySave->Profile.Ranges.Add(TEXT("EyeWideLeft"),Legacy);
    TArray<uint8> Bytes;TestTrue(TEXT("Legacy default profile serializes"),UGameplayStatics::SaveGameToMemory(LegacySave,Bytes));
    if(auto* Loaded=Cast<UVFNCalibrationSave>(UGameplayStatics::LoadGameFromMemory(Bytes)))
    {TestEqual(TEXT("Omitted version survives binary loading"),Loaded->Profile.SchemaVersion,2);TestTrue(TEXT("Loaded legacy range retains fourfold response"),FMath::IsNearlyEqual(UVFNCalibrationLibrary::ApplyProfile(F,Loaded->Profile).Curves.FindRef(TEXT("EyeWideLeft")),.6f));}
    else AddError(TEXT("Legacy default profile failed to load"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNGuideFlowTest,"VRMFaceNodes.GuidedFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNGuideFlowTest::RunTest(const FString&)
{
    const UWorld::InitializationValues Init=UWorld::InitializationValues().CreateAISystem(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Guide=World->SpawnActor<AVRMFaceCalibrationGuide>();
    Guide->bPromptSound=false;Guide->bShowInterface=false;Guide->bAutomaticInput=false;
    Guide->StartCalibration();
    FVFNFrame F;F.bValid=true;F.bHasHead=true;F.SourceTimestamp=1.;
    for(FName N:UVRMFaceLibrary::SemanticNames()) F.Curves.Add(N,.02f);
    Guide->FeedFrame(F,1.f/30.f);
    for(int32 I=0;I<60;++I) Guide->FeedFrame(F,1.f/30.f);
    TestEqual(TEXT("Paused input does not advance collection"),Guide->Progress,0.f);
    TestFalse(TEXT("Repeated timestamp is not fresh"),Guide->bFreshInput);
    const auto Steps=Guide->GetSteps();
    int32 FrameIndex=0;
    auto Sample=[&](bool OneEyeOnly)
    {
        ++FrameIndex;F.SourceTimestamp+=1./30.;F.Head=FRotator::ZeroRotator;
        for(auto& P:F.Curves) P.Value=.02f;
        const int32 Step=Guide->StepIndex;
        if(Steps.IsValidIndex(Step) && Step>0)
            for(FName N:Steps[Step].Channels) F.Curves[N]=OneEyeOnly && N==TEXT("EyeBlinkRight")?.02f:.8f;
        if(Step==1 && Guide->JawPhase==0)F.Curves[TEXT("JawOpen")]=.35f;
        if(Step==Steps.Num()-1) F.Head=FRotator(FMath::Sin(FrameIndex*.05f)*6.f,FMath::Sin(FrameIndex*.08f)*10.f,0);
        Guide->FeedFrame(F,1.f/30.f);
    };
    for(int32 I=0;I<5000 && Guide->bRunning;++I) Sample(false);
    TestTrue(TEXT("All 17 steps finish using fresh input"),Guide->bComplete);
    TestTrue(TEXT("Neutral samples retained"),Guide->Candidate.NeutralSamples>=100);
    TestTrue(TEXT("Both eyelids fitted"),Guide->Candidate.Ranges.Contains(TEXT("EyeBlinkLeft")) && Guide->Candidate.Ranges.Contains(TEXT("EyeBlinkRight")));
    TestTrue(TEXT("Head motion measured"),Guide->Candidate.bMotionMeasured);
    FString Message;TestTrue(TEXT("Full guide produces a valid profile"),UVFNCalibrationLibrary::ValidateProfile(Guide->Candidate,Message));
    if(!Guide->Candidate.Ranges.Contains(TEXT("JawOpen")))
    {AddError(TEXT("Full guide did not produce a jaw range"));Guide->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
    const auto PreviousJaw=Guide->Candidate.Ranges.FindChecked(TEXT("JawOpen"));
    FVFNMorphTweak Tweak;Tweak.Gain=.42f;Guide->Candidate.MorphTweaks.Add(TEXT("mouthSmileLeft"),Tweak);
    Guide->StartSingleStep(2);
    for(int32 I=0;I<1000 && Guide->bRunning;++I)Sample(false);
    TestTrue(TEXT("Single action completes without full wizard"),Guide->bComplete);
    TestEqual(TEXT("Single action preserves other calibration"),Guide->Candidate.Ranges.FindChecked(TEXT("JawOpen")).Maximum,PreviousJaw.Maximum);
    TestEqual(TEXT("Single action preserves per-morph tuning"),Guide->Candidate.MorphTweaks.FindChecked(TEXT("mouthSmileLeft")).Gain,.42f);
    Guide->Candidate.Ranges[TEXT("EyeBlinkRight")].Maximum=.55f;
    Guide->StartCalibration();
    for(int32 I=0;I<5000 && Guide->bRunning;++I) Sample(true);
    TestTrue(TEXT("One-sided capture no longer blocks the full wizard"),Guide->bComplete);
    TestTrue(TEXT("Valid side is updated"),Guide->Candidate.Ranges.Contains(TEXT("EyeBlinkLeft")));
    TestEqual(TEXT("Unmeasured side retains existing range"),Guide->Candidate.Ranges[TEXT("EyeBlinkRight")].Maximum,.55f);
    TestTrue(TEXT("Summary names the preserved side"),Guide->CalibrationDetails.Contains(TEXT("右侧闭眼")));
    const float InnerBefore=Guide->Candidate.Ranges[TEXT("BrowInnerUp")].Maximum;
    Guide->StartSingleStep(4);int32 NeutralFrame=0;
    for(int32 I=0;I<1000 && Guide->bRunning;++I)
    {
        F.SourceTimestamp+=1./30.;F.Head=FRotator(0,0,0);
        for(auto& P:F.Curves)P.Value=.02f;
        F.Curves[TEXT("BrowInnerUp")]=.45f; // high natural baseline is valid
        F.Curves[TEXT("BrowOuterUpLeft")]=Guide->StepIndex==4?.35f:.3f;
        F.Curves[TEXT("BrowOuterUpRight")]=Guide->StepIndex==4?.38f:.3f;
        if(Guide->StepIndex==0)
        {
            ++NeutralFrame;const bool Blink=NeutralFrame%60<10;
            F.Curves[TEXT("EyeBlinkLeft")]=F.Curves[TEXT("EyeBlinkRight")]=Blink?.9f:.02f;
        }
        Guide->FeedFrame(F,1.f/30.f);
    }
    TestTrue(TEXT("Natural blinks and weak outer-brow movement complete calibration"),Guide->bComplete);
    TestEqual(TEXT("Flat inner-brow channel cannot veto outer brows or erase old data"),Guide->Candidate.Ranges[TEXT("BrowInnerUp")].Maximum,InnerBefore);
    TestTrue(TEXT("Small but clean left brow range accepted"),FMath::IsNearlyEqual(Guide->Candidate.Ranges[TEXT("BrowOuterUpLeft")].Maximum,.35f));
    TestTrue(TEXT("Independent right brow range accepted"),FMath::IsNearlyEqual(Guide->Candidate.Ranges[TEXT("BrowOuterUpRight")].Maximum,.38f));
    TestTrue(TEXT("New wizard profile validates"),UVFNCalibrationLibrary::ValidateProfile(Guide->Candidate,Message));
    const auto Calibrated=UVFNCalibrationLibrary::ApplyProfile(F,Guide->Candidate);
    TestTrue(TEXT("Comfortable weak brow maximum reaches full range"),Calibrated.Curves.FindRef(TEXT("BrowOuterUpLeft"))>.99f);
    TestEqual(TEXT("Other calibration still preserved"),Guide->Candidate.Ranges.FindChecked(TEXT("JawOpen")).Maximum,PreviousJaw.Maximum);
    Guide->CancelCalibration();TestFalse(TEXT("Cancel stops sampling"),Guide->bRunning);
    Guide->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNQuickCalibrationTest,"VRMFaceNodes.QuickCalibration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNQuickCalibrationTest::RunTest(const FString&)
{
    const UWorld::InitializationValues Init=UWorld::InitializationValues().CreateAISystem(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Guide=World->SpawnActor<AVRMFaceCalibrationGuide>();
    Guide->bPromptSound=false;Guide->bShowInterface=false;Guide->bAutomaticInput=false;
    const TArray<FName> Core={TEXT("JawOpen"),TEXT("EyeBlinkLeft"),TEXT("EyeBlinkRight"),TEXT("BrowInnerUp"),TEXT("BrowOuterUpLeft"),TEXT("BrowOuterUpRight")};
    FVFNPerformerProfile Before;Before.Name=TEXT("Quick calibration test");Before.Subject=Guide->Subject;Before.NeutralSamples=100;
    Before.bOverrideMirror=true;Before.bMirrorCapture=false;Before.bMotionMeasured=true;Before.MotionStart=63.f;Before.MotionFull=184.f;
    Before.bRecenterHead=true;Before.NeutralHead=FRotator(3,-10,7);Before.ModelPath=TEXT("/Game/TestModel");Before.SavedAt=TEXT("unchanged");
    FVFNCurveRange Existing;Existing.Neutral=.01f;Existing.RestCeiling=.02f;Existing.Maximum=.55f;Existing.bEnabled=true;Existing.SampleCount=100;
    for(FName N:Core)Before.Ranges.Add(N,Existing);
    FVFNCurveRange Cheek=Existing;Cheek.Maximum=.31f;Before.Ranges.Add(TEXT("CheekPuff"),Cheek);
    for(FName N:{FName(TEXT("EyeWideLeft")),FName(TEXT("MouthLowerDownLeft")),FName(TEXT("MouthLowerDownRight")),FName(TEXT("MouthSmileLeft")),FName(TEXT("MouthRollUpper"))})Before.Ranges.Add(N,Existing);
    FVFNMorphTweak Tweak;Tweak.Gain=.42f;Tweak.Offset=.03f;Tweak.Maximum=.7f;Before.MorphTweaks.Add(TEXT("mouthSmileLeft"),Tweak);
    Guide->Candidate=Before;
    Guide->StartQuickCalibration();
    TestTrue(TEXT("Quick prompt starts at the first of four steps"),Guide->GetPrompt().Contains(TEXT("01 / 04")) && Guide->GetPrompt().Contains(TEXT("自然表情")));
    FVFNFrame Frame;Frame.bValid=true;Frame.bHasHead=true;Frame.SourceTimestamp=1.;Frame.Head=FRotator(20,40,-10);
    const auto Steps=Guide->GetSteps();
    TestEqual(TEXT("The public action table still has all legacy entries"),Steps.Num(),17);
    TArray<int32> Visited;bool CheckedSecond=false,CheckedRepeatedStart=false;
    auto Sample=[&]()
    {
        const int32 Step=Guide->StepIndex;
        if(Visited.IsEmpty() || Visited.Last()!=Step)Visited.Add(Step);
        if(Step==2 && !CheckedSecond)
        {CheckedSecond=true;TestTrue(TEXT("Quick prompt uses progress order rather than the legacy action index"),Guide->GetPrompt().Contains(TEXT("02 / 04")) && Guide->GetPrompt().Contains(TEXT("双眼闭合")));}
        if(Step==2 && Guide->Progress>.2f && !CheckedRepeatedStart)
        {
            CheckedRepeatedStart=true;const float ProgressBefore=Guide->Progress;
            Guide->StartQuickCalibration();Guide->StartCalibration();Guide->StartSingleStep(1);
            TestEqual(TEXT("Repeated starts cannot replace an active quick action"),Guide->StepIndex,2);
            TestEqual(TEXT("Repeated starts cannot reset collected progress"),Guide->Progress,ProgressBefore);
        }
        Frame.Curves.Empty();for(FName N:UVRMFaceLibrary::SemanticNames())Frame.Curves.Add(N,.02f);
        if(Steps.IsValidIndex(Step) && Step>0)for(FName N:Steps[Step].Channels)Frame.Curves[N]=.8f;
        if(Step==1 && Guide->JawPhase==0)Frame.Curves[TEXT("JawOpen")]=.35f;
        // Missing input differs from an intentionally open eye. Keep its saved
        // calibration while fitting the left eye, and do not invent a right peak.
        Frame.Curves.Remove(TEXT("EyeBlinkRight"));
        Frame.SourceTimestamp+=1./30.;Guide->FeedFrame(Frame,1.f/30.f);
    };
    for(int32 I=0;I<1800 && Guide->bRunning;++I)Sample();
    TestTrue(TEXT("Quick calibration completes without optional actions"),Guide->bComplete);
    TestTrue(TEXT("Quick sequence is neutral, closed eyes, jaw, then brows"),Visited==TArray<int32>({0,2,1,4}));
    TestTrue(TEXT("The repeated-start guard was exercised during collection"),CheckedRepeatedStart);
    TestEqual(TEXT("Quick candidate keeps six core channels plus previously measured cheek puff"),Guide->Candidate.Ranges.Num(),7);
    TestTrue(TEXT("Quick calibration preserves all fields of the measured cheek range"),FVFNCurveRange::StaticStruct()->CompareScriptStruct(&Guide->Candidate.Ranges.FindChecked(TEXT("CheekPuff")),&Cheek,PPF_None));
    for(FName N:Core)
    {
        if(const auto* R=Guide->Candidate.Ranges.Find(N))
            TestTrue(FString::Printf(TEXT("Core range %s is fitted or retained when missing"),*N.ToString()),FMath::IsNearlyEqual(R->Maximum,N==TEXT("EyeBlinkRight")?.55f:.8f));
        else AddError(FString::Printf(TEXT("Missing core range %s"),*N.ToString()));
    }
    TestFalse(TEXT("Quick jaw action does not normalize left lower-lip movement"),Guide->Candidate.Ranges.Contains(TEXT("MouthLowerDownLeft")));
    TestFalse(TEXT("Quick jaw action does not normalize right lower-lip movement"),Guide->Candidate.Ranges.Contains(TEXT("MouthLowerDownRight")));
    TestTrue(TEXT("Missing eye is named in the completion feedback"),Guide->CalibrationDetails.Contains(TEXT("右侧闭眼")));
    FVFNFrame Probe;Probe.bValid=true;Probe.Curves={{TEXT("EyeWideLeft"),.3f},{TEXT("MouthLowerDownLeft"),.3f},{TEXT("MouthSmileLeft"),.3f},{TEXT("MouthRollUpper"),.3f}};
    const auto Applied=UVFNCalibrationLibrary::ApplyProfile(Probe,Guide->Candidate);
    for(const auto& Pair:Probe.Curves)
    {
        TestEqual(FString::Printf(TEXT("Non-core channel %s keeps its source value"),*Pair.Key.ToString()),Applied.Curves.FindRef(Pair.Key),Pair.Value);
        TestFalse(FString::Printf(TEXT("Non-core channel %s is not flagged as calibrated"),*Pair.Key.ToString()),Applied.PerformerCalibratedChannels.Contains(Pair.Key));
    }
    TestTrue(TEXT("Quick calibration preserves the explicit mirror choice"),Guide->Candidate.bOverrideMirror && !Guide->Candidate.bMirrorCapture);
    TestTrue(TEXT("Quick calibration preserves measured motion thresholds"),Guide->Candidate.bMotionMeasured && Guide->Candidate.MotionStart==Before.MotionStart && Guide->Candidate.MotionFull==Before.MotionFull);
    TestTrue(TEXT("Quick recalibration preserves the existing head center"),Guide->Candidate.bRecenterHead && Guide->Candidate.NeutralHead.Equals(Before.NeutralHead));
    TestEqual(TEXT("Quick calibration preserves model-specific tuning"),Guide->Candidate.ModelPath,Before.ModelPath);
    if(const auto* Preserved=Guide->Candidate.MorphTweaks.Find(TEXT("mouthSmileLeft")))
        TestTrue(TEXT("Quick calibration preserves morph gain, offset, and cap"),Preserved->Gain==Tweak.Gain && Preserved->Offset==Tweak.Offset && Preserved->Maximum==Tweak.Maximum);
    else AddError(TEXT("Quick calibration erased the existing morph tweak"));
    TestFalse(TEXT("Quick completion does not automatically apply a preview"),Guide->bPreviewing);
    TestEqual(TEXT("Quick completion does not change the save timestamp"),Guide->Candidate.SavedAt,Before.SavedAt);
    FString Message;TestTrue(TEXT("Quick candidate is a valid profile"),UVFNCalibrationLibrary::ValidateProfile(Guide->Candidate,Message));
    Guide->CancelCalibration();
    TestEqual(TEXT("Cancel restores the original number of calibrated channels"),Guide->Candidate.Ranges.Num(),Before.Ranges.Num());
    TestEqual(TEXT("Cancel restores the original format version"),Guide->Candidate.SchemaVersion,Before.SchemaVersion);
    TestEqual(TEXT("Cancel restores the original neutral sample count"),Guide->Candidate.NeutralSamples,Before.NeutralSamples);
    for(const auto& Pair:Before.Ranges)
    {
        if(const auto* Restored=Guide->Candidate.Ranges.Find(Pair.Key))
        {const auto& R=Pair.Value;TestTrue(FString::Printf(TEXT("Cancel restores every field of %s"),*Pair.Key.ToString()),Restored->Neutral==R.Neutral && Restored->RestCeiling==R.RestCeiling && Restored->Maximum==R.Maximum && Restored->bEnabled==R.bEnabled && Restored->SampleCount==R.SampleCount && Restored->MaxGain==R.MaxGain);}
        else AddError(FString::Printf(TEXT("Cancel did not restore %s"),*Pair.Key.ToString()));
    }
    Guide->PreviewEssentialCalibration();
    TestFalse(TEXT("Essential preview does not ask the performer to record again"),Guide->bRunning);
    TestEqual(TEXT("Essential preview keeps six core ranges and existing cheek puff"),Guide->Candidate.Ranges.Num(),7);
    TestTrue(TEXT("Essential preview preserves all fields of the measured cheek range"),FVFNCurveRange::StaticStruct()->CompareScriptStruct(&Guide->Candidate.Ranges.FindChecked(TEXT("CheekPuff")),&Cheek,PPF_None));
    for(FName N:Core)
    {
        if(const auto* R=Guide->Candidate.Ranges.Find(N))TestEqual(FString::Printf(TEXT("Essential preview retains existing %s measurement"),*N.ToString()),R->Maximum,Before.Ranges.FindChecked(N).Maximum);
        else AddError(FString::Printf(TEXT("Essential preview removed core range %s"),*N.ToString()));
    }
    TestEqual(TEXT("Essential preview leaves the saved timestamp untouched"),Guide->Candidate.SavedAt,Before.SavedAt);
    Guide->PreviewEssentialCalibration();
    TestEqual(TEXT("Repeated essential preview does not replace its restore point"),Guide->Candidate.Ranges.Num(),7);
    Guide->CancelCalibration();
    TestEqual(TEXT("Cancel essential preview restores every original range"),Guide->Candidate.Ranges.Num(),Before.Ranges.Num());
    for(const auto& Pair:Before.Ranges)
    {
        if(const auto* R=Guide->Candidate.Ranges.Find(Pair.Key))TestEqual(FString::Printf(TEXT("Cancel essential preview restores %s"),*Pair.Key.ToString()),R->Maximum,Pair.Value.Maximum);
        else AddError(FString::Printf(TEXT("Cancel essential preview lost %s"),*Pair.Key.ToString()));
    }
    // A brand-new performer still gets a useful head reference from the same
    // neutral stage, without adding a separate head-motion action.
    Guide->Candidate={};Guide->StartQuickCalibration();
    for(int32 I=0;I<400 && Guide->bRunning && Guide->StepIndex==0;++I)Sample();
    TestEqual(TEXT("Fresh profile proceeds directly from neutral to closed eyes"),Guide->StepIndex,2);
    TestTrue(TEXT("Fresh quick profile measures its natural head center"),Guide->Candidate.bRecenterHead && Guide->Candidate.NeutralHead.Equals(Frame.Head,.001f));
    TestFalse(TEXT("Quick calibration does not invent an unmeasured cheek range"),Guide->Candidate.Ranges.Contains(TEXT("CheekPuff")));
    Guide->CancelCalibration();
    // Switching producer formats invalidates the old source-specific ranges,
    // motion thresholds, and head baseline even in the short workflow.
    Guide->Candidate=Before;Frame.bMetaHuman=true;Guide->StartQuickCalibration();
    for(int32 I=0;I<400 && Guide->bRunning && Guide->StepIndex==0;++I)Sample();
    TestEqual(TEXT("Changed-source quick calibration finishes its new neutral stage"),Guide->StepIndex,2);
    TestTrue(TEXT("Changed-source quick profile uses the new source format"),Guide->Candidate.bMetaHuman);
    TestTrue(TEXT("Changed-source quick profile uses the newly measured head center"),Guide->Candidate.bRecenterHead && Guide->Candidate.NeutralHead.Equals(Frame.Head,.001f));
    TestFalse(TEXT("Changed-source quick profile cannot restore the old head center"),Guide->Candidate.NeutralHead.Equals(Before.NeutralHead,.001f));
    TestTrue(TEXT("Changed-source quick profile clears old facial ranges before fitting"),Guide->Candidate.Ranges.IsEmpty());
    TestTrue(TEXT("Changed-source quick profile clears old measured motion thresholds"),!Guide->Candidate.bMotionMeasured && Guide->Candidate.MotionStart==90.f && Guide->Candidate.MotionFull==250.f);
    Guide->CancelCalibration();
    TestFalse(TEXT("Cancel changed-source calibration restores the original format"),Guide->Candidate.bMetaHuman);
    TestTrue(TEXT("Cancel changed-source calibration restores the original motion and head settings"),Guide->Candidate.bMotionMeasured && Guide->Candidate.MotionStart==Before.MotionStart && Guide->Candidate.MotionFull==Before.MotionFull && Guide->Candidate.NeutralHead.Equals(Before.NeutralHead));
    Guide->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNFastTeethTest,"VRMFaceNodes.FastTeethAndSmile",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNFastTeethTest::RunTest(const FString&)
{
    for(int32 FPS:{30,60,120})
    {
        FVFNSettings S;S.bMirrorCapture=false;S.bSwapEyes=false;FVFNState State;FVFNFrame F;F.bValid=true;F.bHasHead=true;
        F.Curves={{TEXT("MouthRight"),0.f},{TEXT("MouthSmileLeft"),.8f}};
        auto R=UVRMFaceLibrary::RetractTeeth(F,S,State,1.f/FPS);R=UVRMFaceLibrary::Stabilize(R,S,State,1.f/FPS);
        F.Curves[TEXT("MouthRight")]=1.f;F.Head.Yaw=20.f;
        R=UVRMFaceLibrary::RetractTeeth(F,S,State,1.f/FPS);R=UVRMFaceLibrary::Stabilize(R,S,State,1.f/FPS);
        TestEqual(TEXT("Full correction in the very first sudden-sideways frame"),R.Curves.FindRef(TEXT("VRoid_TeethRetract")),.6f);
        for(int32 I=0;I<60;++I){F.Head.Yaw+=5.f;R=UVRMFaceLibrary::RetractTeeth(F,S,State,1.f/FPS);R=UVRMFaceLibrary::Stabilize(R,S,State,1.f/FPS);}
        F.Curves[TEXT("MouthRight")]=0.f;R=UVRMFaceLibrary::RetractTeeth(F,S,State,1.f/FPS);R=UVRMFaceLibrary::Stabilize(R,S,State,1.f/FPS);
        const float V=FMath::Clamp((R.Curves.FindRef(TEXT("MouthRight"))-.4f)/.4f,0.f,1.f);
        TestTrue(TEXT("Retraction remains sufficient for still-displaced displayed mouth"),R.Curves.FindRef(TEXT("VRoid_TeethRetract"))+.0001f>=V*V*(3-2*V)*.6f);
        R=UVRMFaceLibrary::Calibrate(F,S,State,1.f/FPS);TestTrue(TEXT("Smile output is 70 percent"),FMath::IsNearlyEqual(R.Curves.FindRef(TEXT("MouthSmileLeft")),.56f,.0001f));
    }
    FVFNMorphTweak T;T.Gain=.5f;T.Offset=.1f;T.Maximum=.7f;
    TestEqual(TEXT("Per-morph gain and offset"),UVFNCalibrationLibrary::ApplyMorphTweak(.8f,T),.5f);
    TestEqual(TEXT("Per-morph clamp"),UVFNCalibrationLibrary::ApplyMorphTweak(2.f,T),.7f);
    T.bEnabled=false;TestEqual(TEXT("Disabled morph produces zero"),UVFNCalibrationLibrary::ApplyMorphTweak(1.f,T),0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNMirrorTest,"VRMFaceNodes.UnifiedMirror",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNMirrorTest::RunTest(const FString&)
{
    FVFNSettings S;S.bVRoidAxes=false;S.bMirrorCapture=true;S.bSwapEyes=true;S.bCalibration=false;FVFNState State;FVFNFrame F;F.bValid=true;F.bHasHead=true;F.Head=FRotator(10,20,30);
    F.Curves={{TEXT("BrowOuterUpLeft"),.8f},{TEXT("BrowOuterUpRight"),.1f},{TEXT("EyeBlinkLeft"),.7f},{TEXT("EyeBlinkRight"),.2f},{TEXT("EyeLookOutLeft"),.9f},{TEXT("EyeLookInRight"),.9f},{TEXT("MouthRight"),.6f},{TEXT("MouthSmileLeft"),.5f}};
    F.PerformerCalibratedChannels.Add(TEXT("EyeBlinkLeft"));
    auto R=UVRMFaceLibrary::Calibrate(F,S,State,1.f/60.f);
    TestEqual(TEXT("Brows mirrored"),R.Curves.FindRef(TEXT("BrowOuterUpRight")),.8f);
    TestEqual(TEXT("Blink mirrored once despite legacy eye option"),R.Curves.FindRef(TEXT("EyeBlinkRight")),.7f);
    TestEqual(TEXT("Gaze mirrors together with face"),R.Curves.FindRef(TEXT("EyeLookOutRight")),.9f);
    TestEqual(TEXT("Sideways mouth mirrored"),R.Curves.FindRef(TEXT("MouthLeft")),.6f);
    TestEqual(TEXT("Smile side mirrored"),R.Curves.FindRef(TEXT("MouthSmileRight")),.5f);
    TestTrue(TEXT("Head yaw and roll mirrored, pitch retained"),R.Head.Equals(FRotator(10,-20,-30)));
    TestTrue(TEXT("Calibration flags follow eyelid side"),R.PerformerCalibratedChannels.Contains(TEXT("EyeBlinkRight"))&&!R.PerformerCalibratedChannels.Contains(TEXT("EyeBlinkLeft")));
    auto Twice=UVRMFaceLibrary::Calibrate(R,S,State,1.f/60.f);TestTrue(TEXT("Twice mirrored restores head"),Twice.Head.Equals(F.Head));
    TestEqual(TEXT("Twice mirrored restores sparse mouth channel"),Twice.Curves.FindRef(TEXT("MouthRight")),.6f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNVRoidHeadMirrorTest,"VRMFaceNodes.VRoidHeadMirrorAxes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNVRoidHeadMirrorTest::RunTest(const FString&)
{
    const auto ReflectX=[](FVector V){V.X=-V.X;return V;};
    // Independently transcribed from the user's current HeadPosition Blueprint
    // (its legacy Mirror variable is false): Pitch=-rawPitch*.8,
    // Yaw=rawRoll*.8, Roll=rawYaw. Test rendered component rotations, not raw labels.
    for(const FRotator Raw:{FRotator(12,0,0),FRotator(-12,0,0),FRotator(0,12,0),
        FRotator(0,-12,0),FRotator(0,0,12),FRotator(0,0,-12),FRotator(9,-14,18)})
    {
        FVFNSettings S;S.bVRoidAxes=true;S.bMirrorCapture=true;S.bSwapEyes=false;S.bCalibration=false;
        FVFNFrame F;F.bValid=true;F.bHasHead=true;F.Head=Raw;
        F.Curves={{TEXT("EyeBlinkLeft"),.8f},{TEXT("EyeBlinkRight"),.1f},
            {TEXT("BrowOuterUpLeft"),.7f},{TEXT("EyeLookOutLeft"),.9f},{TEXT("MouthRight"),.6f}};
        FVFNState Mirrored,Direct;
        auto M=UVRMFaceLibrary::Calibrate(F,S,Mirrored,1.f/60.f);
        UVRMFaceLibrary::FollowHead(M,S,Mirrored,1.f/60.f);
        const FRotator Manual(-Raw.Pitch*.8,Raw.Roll*.8,Raw.Yaw);
        TestTrue(TEXT("Mirror on matches working manual head direction"),Mirrored.SmoothedHead.Equals(Manual,.0001));
        TestEqual(TEXT("Blink remains mirrored"),M.Curves.FindRef(TEXT("EyeBlinkRight")),.8f);
        TestEqual(TEXT("Brow remains mirrored"),M.Curves.FindRef(TEXT("BrowOuterUpRight")),.7f);
        TestEqual(TEXT("Gaze remains mirrored"),M.Curves.FindRef(TEXT("EyeLookOutRight")),.9f);
        TestEqual(TEXT("Mouth remains mirrored"),M.Curves.FindRef(TEXT("MouthLeft")),.6f);
        S.bMirrorCapture=false;
        auto D=UVRMFaceLibrary::Calibrate(F,S,Direct,1.f/60.f);
        UVRMFaceLibrary::FollowHead(D,S,Direct,1.f/60.f);
        TestTrue(TEXT("Nod unchanged when toggling mirror"),FMath::IsNearlyEqual(Direct.SmoothedHead.Roll,Mirrored.SmoothedHead.Roll,.0001));
        for(const FVector V:{FVector(1,0,0),FVector(0,1,0),FVector(0,0,1)})
        {
            TestTrue(TEXT("Entire head orientation reflects left/right in model space"),
                Direct.SmoothedHead.RotateVector(V).Equals(ReflectX(Mirrored.SmoothedHead.RotateVector(ReflectX(V))),.0001));
            TestTrue(TEXT("Upper body follows the same reflection"),
                Direct.SmoothedBody.RotateVector(V).Equals(ReflectX(Mirrored.SmoothedBody.RotateVector(ReflectX(V))),.0001));
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNVRoidEyeMirrorTest,"VRMFaceNodes.VRoidEyeMirrorAxes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNVRoidEyeMirrorTest::RunTest(const FString&)
{
    // Gold reference: the user's unchanged manual EventGraph uses
    // yaw=(LookLeft-LookRight)*sensitivity, roll=(LookDown-LookUp)*sensitivity,
    // and assigns each source eye to the opposite bone for the desired mirror.
    // Unequal eye magnitudes catch a missing side swap as well as wrong signs.
    for(bool Mirror:{true,false})for(const FVector2D Direction:{FVector2D(-1,0),FVector2D(1,0),
        FVector2D(0,-1),FVector2D(0,1),FVector2D(-1,-1),FVector2D(-1,1),FVector2D(1,-1),FVector2D(1,1)})
    {
        TMap<FName,float> Raw;
        for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
        {
            const float Gain=FCString::Strcmp(Side,TEXT("L"))==0?.8f:.45f;
            const auto Add=[&](const TCHAR* DirectionName,float Value){Raw.Add(FName(FString(TEXT("CTRL_expressions_eyeLook"))+DirectionName+Side),Gain*Value);};
            Add(TEXT("Left"),FMath::Max(0.,-Direction.X));Add(TEXT("Right"),FMath::Max(0.,Direction.X));
            Add(TEXT("Up"),FMath::Max(0.,-Direction.Y));Add(TEXT("Down"),FMath::Max(0.,Direction.Y));
        }
        FVFNSettings S;S.bVRoidAxes=true;S.bMirrorCapture=Mirror;S.bSwapEyes=false;S.bCalibration=false;S.EyeDegrees=15.f;
        FVFNState State;const auto Input=UVRMFaceLibrary::NormalizeCurves(Raw);
        const auto Frame=UVRMFaceLibrary::Calibrate(Input,S,State,1.f/60.f);
        for(bool LeftBone:{true,false})
        {
            const float SourceGain=(Mirror?!LeftBone:LeftBone)?.8f:.45f;
            const FRotator Expected(0.,-Direction.X*15.*SourceGain*(Mirror?1.:-1.),Direction.Y*15.*SourceGain);
            TestTrue(TEXT("All eight actual eye rotation targets match the manual graph or its horizontal reflection"),
                UVRMFaceLibrary::EyeRotation(Frame,S,LeftBone).Equals(Expected,.0001));
        }
    }
    FVFNFrame Generic;Generic.Curves={{TEXT("EyeLookOutLeft"),.8f},{TEXT("EyeLookInRight"),.4f},{TEXT("EyeLookUpLeft"),.3f}};
    FVFNSettings S;S.bVRoidAxes=false;S.EyeDegrees=15.f;
    TestTrue(TEXT("Generic +X rig left eye convention is preserved"),UVRMFaceLibrary::EyeRotation(Generic,S,true).Equals(FRotator(-4.5,12,0),.0001));
    TestTrue(TEXT("Generic +X rig right eye convention is preserved"),UVRMFaceLibrary::EyeRotation(Generic,S,false).Equals(FRotator(0,6,0),.0001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFNProfileManagementTest,"VRMFaceNodes.ProfileManagement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVFNProfileManagementTest::RunTest(const FString&)
{
    auto* GI=NewObject<UGameInstance>();auto* S=NewObject<UVFNCalibrationSubsystem>(GI);
    const FString Prefix=TEXT("VFN_Test_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FName A(*(Prefix+TEXT("_A"))),B(*(Prefix+TEXT("_B"))),C(*(Prefix+TEXT("_C"))),D(*(Prefix+TEXT("_D"))),Binding(*(Prefix+TEXT("_Bind")));
    FVFNPerformerProfile P;P.MorphTweaks.FindOrAdd(TEXT("mouthSmileLeft")).Gain=.7f;FString M;
    TestTrue(TEXT("Tuning-only profile is valid without forcing whole calibration"),UVFNCalibrationLibrary::ValidateProfile(P,M));
    P.SchemaVersion=3;P.NeutralSamples=100;FVFNCurveRange Weak;Weak.Neutral=.35f;Weak.RestCeiling=.354f;Weak.Maximum=.414f;Weak.bEnabled=true;Weak.MaxGain=20.f;
    P.Ranges.Add(TEXT("EyeWideLeft"),Weak);
    TestTrue(TEXT("Create and save profile"),S->Save(A,P,M));TestTrue(TEXT("Saved profiles can be listed"),S->ListProfiles().Contains(A));
    TestTrue(TEXT("Activate profile by binding"),S->Activate(Binding,A,M));TestEqual(TEXT("Binding resolves selection"),S->GetActiveSlot(Binding),A);
    TestTrue(TEXT("Copy profile"),S->CopyProfile(A,B,M));TestFalse(TEXT("Copy refuses overwrite"),S->CopyProfile(A,B,M));
    TestTrue(TEXT("Rename profile"),S->RenameProfile(A,C,M));TestEqual(TEXT("Rename preserves active binding"),S->GetActiveSlot(Binding),C);
    auto* Reloaded=NewObject<UVFNCalibrationSubsystem>(GI);TestEqual(TEXT("Active selection survives subsystem restart"),Reloaded->GetActiveSlot(Binding),C);
    if(const auto* Disk=Reloaded->FindProfile(C))
    {TestEqual(TEXT("Saved calibration upgrades to the simplified profile format"),Disk->SchemaVersion,5);TestEqual(TEXT("Weak-channel gain survives disk reload"),Disk->Ranges.FindChecked(TEXT("EyeWideLeft")).MaxGain,20.f);}
    else AddError(TEXT("Weak-channel profile could not be reloaded"));
    TestTrue(TEXT("Export JSON"),S->ExportProfile(C,M));TestTrue(TEXT("Import JSON under another name"),S->ImportProfile(C,D,M));
    if(const auto* Loaded=S->FindProfile(D))TestEqual(TEXT("Imported tuning retained"),Loaded->MorphTweaks.FindChecked(TEXT("mouthSmileLeft")).Gain,.7f);else AddError(TEXT("Imported profile missing"));
    if(const auto* Loaded=S->FindProfile(D))
    {TestEqual(TEXT("Weak-channel gain survives JSON round trip"),Loaded->Ranges.FindChecked(TEXT("EyeWideLeft")).MaxGain,20.f);TestTrue(TEXT("Weak-channel peak survives JSON round trip"),FMath::IsNearlyEqual(Loaded->Ranges.FindChecked(TEXT("EyeWideLeft")).Maximum,.414f));}
    TestFalse(TEXT("Import rejects path traversal"),S->ImportProfile(TEXT("../x"),A,M));
    TestTrue(TEXT("Delete copied profile"),S->DeleteProfile(B,M));TestTrue(TEXT("Delete renamed profile"),S->DeleteProfile(C,M));TestTrue(TEXT("Delete imported profile"),S->DeleteProfile(D,M));
    IFileManager::Get().Delete(*(FPaths::ProjectSavedDir()/TEXT("VRMFaceNodes/Profiles")/(C.ToString()+TEXT(".json"))));
    return true;
}
#endif
