#include "VRMFaceCalibrationGuide.h"
#include "VRMFaceLibrary.h"
#include "Engine/SkeletalMesh.h"

void AVRMFaceCalibrationGuide::QueueAutoSave()
{
    UVFNCalibrationLibrary::UpgradeProfile(Candidate);
    PreviewCandidate();bAutoSavePending=true;AutoSaveDelay=.5f;
    Status=TEXT("已应用，正在自动保存。");
}
void AVRMFaceCalibrationGuide::StartSimpleCalibration()
{
    if(bRunning||bCenteringCamera)return;
    if(bAutoSavePending){SaveCalibration();bAutoSavePending=false;}
    StartQuickCalibration();bAutoApplySession=bRunning;
}
void AVRMFaceCalibrationGuide::SetEnhancementsEnabled(bool Enabled)
{
    if(bRunning||bCenteringCamera)return;
    Candidate.bEnhancementsEnabled=Enabled;QueueAutoSave();
}
void AVRMFaceCalibrationGuide::SetExpressionAmount(bool Chin,float Amount)
{
    if(bRunning||bCenteringCamera||!FMath::IsFinite(Amount))return;
    if(Chin)Candidate.ChinStrength=FMath::Clamp(Amount,0.f,1.f);
    else Candidate.SpeechAmount=FMath::Clamp(Amount,.5f,1.5f);
    QueueAutoSave();
}
void AVRMFaceCalibrationGuide::RestoreOriginalChin()
{
    SetExpressionAmount(true,0.f);
    if(!bRunning&&!bCenteringCamera)Status=TEXT("已恢复原下巴，其他设置保持。自动保存中。");
}
void AVRMFaceCalibrationGuide::CenterCamera()
{
    if(bRunning||bCenteringCamera)return;
    if(!bFreshInput||!CurrentFrame.bHasHead||CurrentFrame.Head.ContainsNaN())
    {Status=TEXT("请先恢复面捕或回放，再点击校正摄像头。");return;}
    if(!Candidate.Ranges.IsEmpty()&&Candidate.bMetaHuman!=CurrentFrame.bMetaHuman)
    {Status=TEXT("采集类型变了，请先重新校准。");return;}
    CameraHeads.Empty();CameraGaze.Empty();CameraSeconds=0.f;CameraWallSeconds=0.f;
    CameraSubject=Subject;bCameraMetaHuman=CurrentFrame.bMetaHuman;bCenteringCamera=true;
    Status=TEXT("自然坐正看屏幕，保持约一秒。");Signal();
}
void AVRMFaceCalibrationGuide::FeedCamera(const FVFNFrame& F,float Dt,bool NewFrame)
{
    if(!bCenteringCamera)return;
    CameraWallSeconds+=Dt;
    if(Subject!=CameraSubject || (F.bValid&&F.bMetaHuman!=bCameraMetaHuman))
    {bCenteringCamera=false;Status=TEXT("采集来源改变，保留原摄像头校正。");return;}
    if(CameraWallSeconds>4.f)
    {bCenteringCamera=false;Status=TEXT("数据暂停或头部不稳定，保留原校正。恢复后再点一次即可。");return;}
    if(!NewFrame||!F.bHasHead||F.Head.ContainsNaN())return;
    CameraSeconds+=FMath::Min(Dt,.1f);CameraHeads.Add(F.Head);
    for(const TCHAR* Side:{TEXT("Left"),TEXT("Right")})for(bool H:{true,false})
    {
        const FName A(FString(H?TEXT("EyeLookOut"):TEXT("EyeLookUp"))+Side),B(FString(H?TEXT("EyeLookIn"):TEXT("EyeLookDown"))+Side);
        if(F.Curves.Contains(A)&&F.Curves.Contains(B))CameraGaze.FindOrAdd(FName(FString(H?TEXT("Horizontal"):TEXT("Vertical"))+Side)).Add(F.Curves.FindRef(A)-F.Curves.FindRef(B));
    }
    if(CameraSeconds<.7f||CameraHeads.Num()<20)return;
    TArray<float> Pitch,Yaw,Roll;const FRotator Ref=CameraHeads[0];
    for(const FRotator& H:CameraHeads){const auto D=(H-Ref).GetNormalized();Pitch.Add(D.Pitch);Yaw.Add(D.Yaw);Roll.Add(D.Roll);}
    const auto Spread=[](const TArray<float>& V){return UVFNCalibrationLibrary::Percentile(V,.9f)-UVFNCalibrationLibrary::Percentile(V,.1f);};
    if(FMath::Max3(Spread(Pitch),Spread(Yaw),Spread(Roll))>7.f)
    {CameraHeads.Empty();CameraGaze.Empty();CameraSeconds=0.f;Status=TEXT("请保持自然朝向，正在校正。");return;}
    Candidate.NeutralHead=(Ref+FRotator(UVFNCalibrationLibrary::Percentile(Pitch,.5f),UVFNCalibrationLibrary::Percentile(Yaw,.5f),UVFNCalibrationLibrary::Percentile(Roll,.5f))).GetNormalized();
    Candidate.bRecenterHead=true;Candidate.bCameraCentered=true;Candidate.bCameraHeadRadians=false;
    Candidate.bCameraVRoidAxes=!TuningModel || UVRMFaceLibrary::InspectModel(TuningModel).bVRoid;
    Candidate.bMetaHuman=bCameraMetaHuman;Candidate.Subject=Subject;Candidate.NeutralSamples=FMath::Max(Candidate.NeutralSamples,CameraHeads.Num());
    Candidate.NeutralGaze.Empty();
    for(const auto& P:CameraGaze)if(P.Value.Num()>=20)Candidate.NeutralGaze.Add(P.Key,UVFNCalibrationLibrary::Percentile(P.Value,.5f));
    bCenteringCamera=false;QueueAutoSave();Status=TEXT("摄像头已校正，自动保存中。");Signal();
}
