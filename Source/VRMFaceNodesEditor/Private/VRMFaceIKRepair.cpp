#include "VRMFaceIKRepair.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetChainMapping.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigController.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Engine/SkeletalMesh.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Editor.h"

namespace
{
    const FName Legs[]={TEXT("LeftLeg"),TEXT("RightLeg")},Toes[]={TEXT("LeftToe"),TEXT("RightToe")};
    const FName Feet[]={TEXT("foot_l"),TEXT("foot_r")},Balls[]={TEXT("ball_l"),TEXT("ball_r")},Thighs[]={TEXT("thigh_l"),TEXT("thigh_r")};
    const FName VFeet[]={TEXT("J_Bip_L_Foot"),TEXT("J_Bip_R_Foot")},VToes[]={TEXT("J_Bip_L_ToeBase"),TEXT("J_Bip_R_ToeBase")},VThighs[]={TEXT("J_Bip_L_UpperLeg"),TEXT("J_Bip_R_UpperLeg")};
    bool Descends(const FReferenceSkeleton& S,FName Child,FName Parent)
    {int32 I=S.FindBoneIndex(Child),P=S.FindBoneIndex(Parent);if(I==INDEX_NONE || P==INDEX_NONE)return false;for(;I!=INDEX_NONE;I=S.GetParentIndex(I))if(I==P)return true;return false;}
    FName ToeSource(UIKRigController* C,int32 Side)
    {for(const auto& Chain:C->GetRetargetChains())if(Chain.StartBone.BoneName==Balls[Side] && Chain.EndBone.BoneName==Balls[Side])return Chain.ChainName;return NAME_None;}
    bool Save(UObject* A)
    {A->MarkPackageDirty();auto* P=A->GetOutermost();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;return UPackage::SavePackage(P,A,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),Args);}
}
FVFNIKRepairResult UVRMFaceIKRepair::Inspect(UIKRetargeter* R)
{
    FVFNIKRepairResult Out;Out.Retargeter=R;
    if(!R || (GEditor && GEditor->PlayWorld)){Out.Notes.Add(TEXT("请停止运行，并选择 VRM4U 生成的 IK 重定向器（RTG）。"));return Out;}
    auto* C=UIKRetargeterController::GetController(R);
    auto* Source=const_cast<UIKRigDefinition*>(C->GetIKRig(ERetargetSourceOrTarget::Source));
    auto* Target=const_cast<UIKRigDefinition*>(C->GetIKRig(ERetargetSourceOrTarget::Target));
    if(!Source || !Target){Out.Notes.Add(TEXT("重定向器缺少源或目标 IK Rig。"));return Out;}
    auto* SC=UIKRigController::GetController(Source);auto* TC=UIKRigController::GetController(Target);Out.SourceRig=Source;
    auto* SM=SC->GetSkeletalMesh();auto* TM=TC->GetSkeletalMesh();
    if(!SM || !TM){Out.Notes.Add(TEXT("源或目标 IK Rig 缺少预览骨骼模型。"));return Out;}
    for(int32 Side=0;Side<2;++Side)
    {
        const auto* SL=SC->GetRetargetChainByName(Legs[Side]);const auto* TL=TC->GetRetargetChainByName(Legs[Side]);const auto* TT=TC->GetRetargetChainByName(Toes[Side]);
        if(!SL || !TL || !TT || SL->StartBone.BoneName!=Thighs[Side] || (SL->EndBone.BoneName!=Feet[Side] && SL->EndBone.BoneName!=Balls[Side]) || TL->StartBone.BoneName!=VThighs[Side] || TL->EndBone.BoneName!=VFeet[Side] || TT->StartBone.BoneName!=VToes[Side] || TT->EndBone.BoneName!=VToes[Side]
            || !Descends(SM->GetRefSkeleton(),Balls[Side],Feet[Side]) || !Descends(SM->GetRefSkeleton(),Feet[Side],Thighs[Side]) || !Descends(TM->GetRefSkeleton(),VToes[Side],VFeet[Side]) || !Descends(TM->GetRefSkeleton(),VFeet[Side],VThighs[Side]))
        {Out.Notes.Add(TEXT("未匹配标准 UE → VRoid 的腿/脚趾链及骨骼层级，已跳过。此按钮不猜测自定义骨架，也不修改骨骼参考姿势。"));return Out;}
        if(SL->EndBone.BoneName!=Feet[Side])Out.bNeedsRepair=true;
        Out.Notes.Add(FString::Printf(TEXT("%s：源链终点 %s → %s；%s 仅匹配同侧脚趾单骨链，缺少则不映射。"),*Legs[Side].ToString(),*SL->EndBone.BoneName.ToString(),*Feet[Side].ToString(),*Toes[Side].ToString()));
    }
    bool HasMaps=false;
    for(int32 I=0;I<C->GetNumRetargetOps();++I)
    {
        const FName Op=C->GetOpName(I);const auto* M=C->GetChainMapping(Op);if(!M)continue;
        if(C->GetTargetIKRigForOp(Op) && C->GetTargetIKRigForOp(Op)!=Target){Out.Notes.Add(TEXT("操作栈使用了额外的目标 IK Rig，未自动修复。请先检查自定义操作。"));return Out;}
        for(int32 Side=0;Side<2;++Side)for(FName Chain:{Legs[Side],Toes[Side]})if(M->HasChain(Chain,ERetargetSourceOrTarget::Target))
        {HasMaps=true;const FName Expected=Chain==Legs[Side]?Legs[Side]:ToeSource(SC,Side);if(C->GetSourceChain(Chain,Op)!=Expected)Out.bNeedsRepair=true;}
    }
    if(!HasMaps){Out.Notes.Add(TEXT("未找到可编辑的链映射操作。"));return Out;}
    Out.bSupported=true;Out.bSuccess=true;
    Out.Notes.Add(Out.bNeedsRepair?TEXT("检测到可修复差异。执行后只生成专用副本，请把动画重定向改用新 RTG。"):TEXT("这四条链已正确，无需修复。"));return Out;
}
FVFNIKRepairResult UVRMFaceIKRepair::Repair(UIKRetargeter* R,const FString& Folder)
{
    auto Out=Inspect(R);if(!Out.bSupported || !Out.bNeedsRepair)return Out;Out.bSuccess=false;
    FText Why;if(!Folder.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(Folder,false,&Why)) {Out.Notes.Add(TEXT("输出目录必须是 /Game/ 下的有效项目目录。"));return Out;}
    auto& Tools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    auto Duplicate=[&](UObject* A,const FString& Base)->UObject*{FString P,N;Tools.CreateUniqueAssetName(Folder/Base,TEXT(""),P,N);return Tools.DuplicateAsset(N,Folder,A);};
    auto* FixedRig=Cast<UIKRigDefinition>(Duplicate(Out.SourceRig,TEXT("IK_VFN_FootFix_")+R->GetName()));
    if(!FixedRig){Out.Notes.Add(TEXT("无法创建源 IK 副本，原资产未变。"));return Out;}
    auto* FC=UIKRigController::GetController(FixedRig);
    for(int32 Side=0;Side<2;++Side)if(!FC->SetRetargetChainEndBone(Legs[Side],Feet[Side])) {Out.Notes.Add(TEXT("副本腿链修改失败，原资产未变。"));return Out;}
    auto* Fixed=Cast<UIKRetargeter>(Duplicate(R,TEXT("RTG_VFN_FootFix_")+R->GetName()));
    if(!Fixed){Out.Notes.Add(TEXT("无法创建重定向器副本，原资产未变。"));return Out;}
    auto* RC=UIKRetargeterController::GetController(Fixed);RC->SetIKRig(ERetargetSourceOrTarget::Source,FixedRig);
    for(int32 Side=0;Side<2;++Side){RC->SetSourceChain(Legs[Side],Legs[Side]);RC->SetSourceChain(ToeSource(FC,Side),Toes[Side]);}
    auto Verify=Inspect(Fixed);
    if(!Verify.bSupported || Verify.bNeedsRepair){Out.Notes.Append(Verify.Notes);Out.Notes.Add(TEXT("副本验证未通过，原重定向器未替换。"));return Out;}
    Out.Retargeter=Fixed;Out.SourceRig=FixedRig;Out.bSuccess=Save(FixedRig)&&Save(Fixed);
    Out.Notes.Add(Out.bSuccess?TEXT("修复副本已保存：")+Fixed->GetPathName()+TEXT("。原 RTG、共用参考 IK、模型骨骼和形态键均未改动。回退时改回原 RTG。"):TEXT("副本保存失败，请查看日志；原资产仍保留。"));return Out;
}

