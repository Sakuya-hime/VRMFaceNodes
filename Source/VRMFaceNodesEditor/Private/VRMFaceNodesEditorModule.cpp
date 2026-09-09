#include "Modules/ModuleManager.h"
#include "VRMFaceEditorLibrary.h"
#include "VRMFaceIKRepair.h"
#include "VRMFaceCalibrationGuide.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "ToolMenus.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetRegistry/AssetData.h"
#include "Styling/CoreStyle.h"
#include "Engine/Selection.h"
#include "Widgets/Layout/SExpandableArea.h"

namespace
{
    const FName TabName(TEXT("VRMFaceNodesSetup"));
    AVRMFaceCalibrationGuide* RunningGuide()
    {
        if(!GEditor || !GEditor->PlayWorld)return nullptr;
        AVRMFaceCalibrationGuide* Only=nullptr;AVRMFaceCalibrationGuide* Selected=nullptr;
        int32 Count=0,SelectedCount=0;
        for(TActorIterator<AVRMFaceCalibrationGuide> It(GEditor->PlayWorld);It;++It)
        {
            if(!It->bShowInterface)continue;
            Only=*It;++Count;
            if(GEditor->GetSelectedActors()->IsSelected(*It)){Selected=*It;++SelectedCount;}
        }
        return SelectedCount==1?Selected:Count==1?Only:nullptr;
    }
    struct FPanelState
    {
        TWeakObjectPtr<USkeletalMesh> Model,ReadyModel;
        TWeakObjectPtr<UAnimBlueprint> ReadyBlueprint;
        TWeakObjectPtr<UIKRetargeter> Retargeter;
        FString Folder=TEXT("/Game/FaceReady"),Subject=TEXT("konohana"),Message=TEXT("选择模型和面捕主题，然后点击接入。已经配置好的场景，运行后从工具 → 面捕设置调整。");
    };
    TSharedRef<STextBlock> Text(const FString& S,int32 Size=16)
    {return SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).AutoWrapText(true).Text(FText::FromString(S));}
    TSharedRef<SWidget> Action(const FString& S,TFunction<void()> F)
    {return SNew(SButton).ContentPadding(FMargin(12,9)).OnClicked_Lambda([F](){F();return FReply::Handled();})[Text(S)];}
}
class FVRMFaceNodesEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if(IsRunningCommandlet())return;
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName,FOnSpawnTab::CreateRaw(this,&FVRMFaceNodesEditorModule::Spawn))
            .SetDisplayName(FText::FromString(TEXT("面捕设置"))).SetMenuType(ETabSpawnerMenuType::Hidden);
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FVRMFaceNodesEditorModule::Menu));
    }
    virtual void ShutdownModule() override
    {
        if(IsRunningCommandlet())return;
        UToolMenus::UnRegisterStartupCallback(this);UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
    }
    void Menu()
    {
        FToolMenuOwnerScoped Owner(this);auto* M=UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
        M->FindOrAddSection(TEXT("VRMFaceNodes")).AddMenuEntry(TEXT("VFNSettings"),FText::FromString(TEXT("面捕设置")),FText::FromString(TEXT("运行时打开独立设置窗口；未配置的模型可在这里一键接入。")),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){if(auto* Guide=RunningGuide())Guide->OpenConfigurationWindow();else FGlobalTabmanager::Get()->TryInvokeTab(TabName);}))); 
    }
    TSharedRef<SDockTab> Spawn(const FSpawnTabArgs&)
    {
        auto P=MakeShared<FPanelState>();auto V=SNew(SVerticalBox);
        V->AddSlot().AutoHeight().Padding(0,0,0,15)[Text(TEXT("面捕设置"),26)];
        V->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("用 VRM4U 导入后，在这里接入面捕。运行场景后，此入口直接打开设置窗口。"))];
        V->AddSlot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("选择模型和面捕主题"),20)];
        V->AddSlot().AutoHeight().Padding(0,0,0,4)[Text(TEXT("骨骼网格体：选择 VRM4U 导入的 SK 模型资产"),14)];
        V->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SObjectPropertyEntryBox).AllowedClass(USkeletalMesh::StaticClass()).ObjectPath_Lambda([P](){return P->Model.IsValid()?P->Model->GetPathName():FString();}).OnObjectChanged_Lambda([P](const FAssetData& A){P->Model=Cast<USkeletalMesh>(A.GetAsset());})];
        V->AddSlot().AutoHeight().Padding(0,0,0,4)[Text(TEXT("面捕主题名：与 Live Link Hub 中的 Subject 完全一致"),14)];
        V->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SEditableTextBox).Text(FText::FromString(P->Subject)).HintText(FText::FromString(TEXT("Live Link Hub 中的主题名"))).OnTextChanged_Lambda([P](const FText& T){P->Subject=T.ToString();})];
        V->AddSlot().AutoHeight().Padding(0,0,0,4)[Text(TEXT("副本保存目录：/Game 表示项目的 Content 文件夹"),14)];
        V->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SEditableTextBox).Text(FText::FromString(P->Folder)).HintText(FText::FromString(TEXT("新资产输出目录，如 /Game/FaceReady"))).OnTextChanged_Lambda([P](const FText& T){P->Folder=T.ToString();})];
        V->AddSlot().AutoHeight().Padding(0,0,0,18)[Action(TEXT("接入面捕并放入场景"),[P](){
            if(!GEditor || GEditor->PlayWorld){P->Message=TEXT("请先停止运行再接入模型。");return;}
            auto R=UVRMFaceEditorLibrary::ConfigureModel(P->Model.Get(),P->Folder,FName(*P->Subject),true);
            P->Message=FString::Join(R.Notes,TEXT("\n"));if(!R.bSuccess)return;
            auto* W=GEditor->GetEditorWorldContext().World();if(!W)return;
            auto* A=W->SpawnActor<ASkeletalMeshActor>();A->SetActorLabel(TEXT("面捕模型"));A->GetSkeletalMeshComponent()->SetSkeletalMeshAsset(R.Model);A->GetSkeletalMeshComponent()->SetAnimInstanceClass(R.AnimationBlueprint->GeneratedClass);
            auto* Guide=W->SpawnActor<AVRMFaceCalibrationGuide>();Guide->Subject=FName(*P->Subject);Guide->TuningModel=R.Model;Guide->SetActorLabel(TEXT("面捕设置"));
            W->MarkPackageDirty();GEditor->SyncBrowserToObjects(TArray<UObject*>{R.Model,R.AnimationBlueprint});P->Message=TEXT("接入完成。保存场景并运行，再从工具 → 面捕设置调整。默认增强全开。");
        })];
        auto Repair=SNew(SVerticalBox);
        Repair->AddSlot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("可选  腿脚 IK 链检查与修复"),20)];
        Repair->AddSlot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("作者设备上曾出现腿链终点落在脚趾、脚趾链误连手臂的问题；不代表所有设备均会出现。工具仅匹配标准 UE → VRoid 的四条链，生成专用副本，保留原资产。"),14)];
        Repair->AddSlot().AutoHeight().Padding(0,0,0,4)[Text(TEXT("IK 重定向器：选择需要检查的 RTG 资产；副本保存到上方目录"),14)];
        Repair->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SObjectPropertyEntryBox).AllowedClass(UIKRetargeter::StaticClass()).ObjectPath_Lambda([P](){return P->Retargeter.IsValid()?P->Retargeter->GetPathName():FString();}).OnObjectChanged_Lambda([P](const FAssetData& A){P->Retargeter=Cast<UIKRetargeter>(A.GetAsset());})];
        Repair->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0)[Action(TEXT("先检查四条链"),[P](){auto R=UVRMFaceIKRepair::Inspect(P->Retargeter.Get());P->Message=FString::Join(R.Notes,TEXT("\n"));})]
            +SHorizontalBox::Slot().AutoWidth()[Action(TEXT("一键修复 · 生成副本"),[P](){auto R=UVRMFaceIKRepair::Repair(P->Retargeter.Get(),P->Folder);P->Message=FString::Join(R.Notes,TEXT("\n"));if(R.bSuccess&&R.bNeedsRepair)GEditor->SyncBrowserToObjects(TArray<UObject*>{R.Retargeter,R.SourceRig});})]];
        V->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[Text(TEXT("腿脚修复"),15)].BodyContent()[Repair]];
        V->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).AutoWrapText(true).Text_Lambda([P](){return FText::FromString(P->Message);})];
        V->AddSlot().AutoHeight()[Text(TEXT("详细步骤、节点说明和适用范围：插件目录 README.zh-CN.md。修复不会自动替换已使用的 RTG；验收新副本后再在重定向流程中选用它。"),14)];
        return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SScrollBox)+SScrollBox::Slot().Padding(24)[SNew(SBox).MaxDesiredWidth(860)[V]]];
    }
};
IMPLEMENT_MODULE(FVRMFaceNodesEditorModule,VRMFaceNodesEditor)
