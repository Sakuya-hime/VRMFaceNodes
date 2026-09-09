#include "VRMFaceCalibrationGuide.h"
#include "VRMFaceLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/MorphTarget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Misc/App.h"

namespace
{
    const FLinearColor Accent(.27f,.85f,.72f),Muted(.7f,.79f,.83f);
    FSlateFontInfo Font(int32 Size) {return FCoreStyle::GetDefaultFontStyle("Regular",Size);}
    TSharedRef<STextBlock> Text(const FString& S,int32 Size=16)
    {return SNew(STextBlock).Font(Font(Size)).Text(FText::FromString(S)).AutoWrapText(true);}
    TSharedRef<SButton> Button(const FString& Label,TFunction<void()> Action)
    {return SNew(SButton).ContentPadding(FMargin(12,8)).OnClicked_Lambda([Action](){Action();return FReply::Handled();})
        [SNew(STextBlock).Font(Font(15)).Text(FText::FromString(Label)).AutoWrapText(false)];}
}
void AVRMFaceCalibrationGuide::RefreshProfiles()
{
    ProfileOptions.Empty();if(auto* S=Service())for(FName N:S->ListProfiles())ProfileOptions.Add(MakeShared<FString>(N.ToString()));
    if(ProfilePicker)ProfilePicker->RefreshOptions();
}
void AVRMFaceCalibrationGuide::RefreshMorphs()
{
    if(!TuningModel && GetWorld())
    {
        TSet<USkeletalMesh*> Models;
        for(TActorIterator<AActor> It(GetWorld());It;++It)
        {TArray<USkeletalMeshComponent*> Components;It->GetComponents(Components);for(auto* C:Components)if(auto* M=C->GetSkeletalMeshAsset())if(M->GetMorphTargets().Num())Models.Add(M);}
        if(Models.Num()==1)TuningModel=*Models.CreateConstIterator();
    }
    MorphOptions.Empty();
    if(TuningModel)for(const auto& M:TuningModel->GetMorphTargets())if(M && (MorphFilter.IsEmpty() || M->GetName().Contains(MorphFilter)))MorphOptions.Add(MakeShared<FString>(M->GetName()));
    MorphOptions.Sort([](const auto& A,const auto& B){return *A<*B;});
    if(SelectedMorph.IsNone() && MorphOptions.Num())SelectedMorph=FName(*MorphOptions[0]);
    if(MorphPicker)MorphPicker->RefreshOptions();
}
FVFNMorphTweak AVRMFaceCalibrationGuide::GetSelectedTweak() const
{if(const auto* T=Candidate.MorphTweaks.Find(SelectedMorph))return *T;return {};}
void AVRMFaceCalibrationGuide::ManageProfile(int32 Action)
{
    auto* S=Service();if(!S || bRunning)return;
    const FName New(*NewProfileName.TrimStartAndEnd());
    if(Action!=4)bPendingDelete=false;
    switch(Action)
    {
    case 0:CreateProfile(NewProfileName);break;
    case 1:SelectProfile(SelectedProfile);break;
    case 2:
        if(!UVFNCalibrationLibrary::IsValidSlot(New) || S->ListProfiles().Contains(New)){Status=TEXT("请填写未使用的新名称。");break;}
        if(S->Save(New,Candidate,Status))SelectProfile(New);break;
    case 3:
        if(S->RenameProfile(SelectedProfile,New,Status)){if(StoredSlot==SelectedProfile)StoredSlot=New;SelectedProfile=New;SelectProfile(New);}break;
    case 4:
        if(!bPendingDelete){bPendingDelete=true;Status=TEXT("将删除「")+SelectedProfile.ToString()+TEXT("」。再次点击删除确认；切换选择可取消。");break;}
        if(S->DeleteProfile(SelectedProfile,Status))
        {bPendingDelete=false;if(StoredSlot==SelectedProfile){StoredSlot=ProfileSlot;Candidate={};bPreviewing=false;if(const auto* P=S->FindProfile(ProfileSlot))Candidate=*P;}SelectedProfile=NAME_None;}break;
    case 5:S->ExportProfile(SelectedProfile,Status);break;
    case 6:if(S->ImportProfile(New,New,Status))SelectProfile(New);break;
    }
    RefreshProfiles();
}

TSharedRef<SWidget> AVRMFaceCalibrationGuide::BuildCalibrationPage()
{
    const TWeakObjectPtr<AVRMFaceCalibrationGuide> W(this);
    auto V=SNew(SVerticalBox);
    V->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(SCheckBox)
        .IsEnabled_Lambda([W](){return W.IsValid()&&!W->bRunning&&!W->bCenteringCamera;})
        .IsChecked_Lambda([W](){return W.IsValid()&&W->Candidate.bEnhancementsEnabled?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([W](ECheckBoxState S){if(W.IsValid())W->SetEnhancementsEnabled(S==ECheckBoxState::Checked);})
        [Text(TEXT("面部增强 · 开启全部效果"),20)]];
    V->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("调好即使用，修改自动保存。关闭增强可对照基础映射。"),14)];
    V->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(SBox).IsEnabled_Lambda([W](){return W.IsValid()&&!W->bRunning&&!W->bCenteringCamera;})
        [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Button(TEXT("重新校准 · 五步"),[W](){if(W.IsValid())W->StartSimpleCalibration();})]
        +SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).ContentPadding(FMargin(12,8)).ToolTipText(FText::FromString(TEXT("自然坐正看屏幕，点击后保持约一秒。校正角色头部和视线朝向，不改变眉眼嘴幅度。严重遮挡仍需调整摄像头位置。")))
            .OnClicked_Lambda([W](){if(W.IsValid())W->CenterCamera();return FReply::Handled();})[Text(TEXT("校正摄像头"),15)]]]];
    V->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(STextBlock).Font(Font(16)).AutoWrapText(true).Text_Lambda([W](){return FText::FromString(W.IsValid()?W->GetPrompt():TEXT(""));})];
    V->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SProgressBar).Visibility_Lambda([W](){return W.IsValid()&&W->bRunning?EVisibility::Visible:EVisibility::Collapsed;})
        .Percent_Lambda([W](){return TOptional<float>(W.IsValid()?W->Progress:0.f);}).FillColorAndOpacity(Accent)];
    V->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SBox).Visibility_Lambda([W](){return W.IsValid()&&W->StepIndex>=0&&!W->bComplete?EVisibility::Visible:EVisibility::Collapsed;})
        [Button(TEXT("取消本次校准"),[W](){if(W.IsValid())W->CancelCalibration();})]];
    return V;
}


TSharedRef<SWidget> AVRMFaceCalibrationGuide::BuildTweaksPage()
{
    const TWeakObjectPtr<AVRMFaceCalibrationGuide> W(this);
    auto V=SNew(SVerticalBox);
    auto Row=[W](const TCHAR* Label,bool Chin)->TSharedRef<SWidget>
    {return SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Label)]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(110)[SNew(SSpinBox<float>).MinValue(Chin?0.f:.5f).MaxValue(Chin?1.f:1.5f).Delta(.01f)
            .Value_Lambda([W,Chin](){return W.IsValid()?(Chin?W->Candidate.ChinStrength:W->Candidate.SpeechAmount):0.f;})
            .OnValueChanged_Lambda([W,Chin](float X){if(W.IsValid())W->SetExpressionAmount(Chin,X);})]];};
    V->AddSlot().AutoHeight().Padding(0,10,0,14)[Row(TEXT("说话张嘴幅度"),false)];
    V->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(SBox).IsEnabled_Lambda([W](){return W.IsValid()&&W->TuningModel&&W->TuningModel->FindMorphTarget(TEXT("VFN_ChinAssist"));})[Row(TEXT("下巴联动强度"),true)]];
    V->AddSlot().AutoHeight().Padding(0,0,0,14)[Button(TEXT("恢复原下巴"),[W](){if(W.IsValid())W->RestoreOriginalChin();})];
    V->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(SCheckBox).IsChecked_Lambda([W](){return W.IsValid()&&W->Candidate.bMirrorCapture?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([W](ECheckBoxState S){if(W.IsValid())W->SetMirrorCapture(S==ECheckBoxState::Checked);})[Text(TEXT("前置镜像 · 左右一致，点头不反转"),14)]];
    auto Details=SNew(SVerticalBox);
    Details->AddSlot().AutoHeight().Padding(0,10,0,10)[SNew(SSearchBox).HintText(FText::FromString(TEXT("搜索表情名称"))).OnTextChanged_Lambda([W](const FText& T){if(W.IsValid()){W->MorphFilter=T.ToString();W->RefreshMorphs();}})];
    Details->AddSlot().AutoHeight().Padding(0,0,0,10)[SAssignNew(MorphPicker,SComboBox<TSharedPtr<FString>>).OptionsSource(&MorphOptions).MaxListHeight(200)
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> S)->TSharedRef<SWidget>{return Text(S.IsValid()?*S:TEXT(""),14);})
        .OnSelectionChanged_Lambda([W](TSharedPtr<FString> S,ESelectInfo::Type){if(W.IsValid()&&S.IsValid())W->SelectedMorph=FName(**S);})
        [SNew(STextBlock).Font(Font(14)).Text_Lambda([W](){return FText::FromName(W.IsValid()?W->SelectedMorph:NAME_None);})]];
    Details->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("此表情幅度"),14)]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(110)[SNew(SSpinBox<float>).MinValue(0.f).MaxValue(3.f).Delta(.01f)
            .Value_Lambda([W](){return W.IsValid()?W->GetSelectedTweak().Gain:1.f;})
            .OnValueChanged_Lambda([W](float X){if(W.IsValid()){auto T=W->GetSelectedTweak();T.Gain=X;W->SetMorphTweak(W->SelectedMorph,T);}})]]];
    Details->AddSlot().AutoHeight()[Button(TEXT("恢复此表情"),[W](){if(W.IsValid())W->ResetMorphTweak(W->SelectedMorph);})];
    V->AddSlot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[Text(TEXT("更多表情幅度"),14)].BodyContent()[Details]];
    return SNew(SBox).IsEnabled_Lambda([W](){return W.IsValid()&&!W->bRunning&&!W->bCenteringCamera&&W->Candidate.bEnhancementsEnabled;})[V];
}

TSharedRef<SWidget> AVRMFaceCalibrationGuide::BuildProfilesPage()
{
    const TWeakObjectPtr<AVRMFaceCalibrationGuide> W(this);
    TSharedRef<SVerticalBox> V=SNew(SVerticalBox);
    V->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(TEXT("选择即使用。日常修改自动保存，可将不同人的配置分别命名。"))];
    V->AddSlot().AutoHeight().Padding(0,0,0,10)[SAssignNew(ProfilePicker,SComboBox<TSharedPtr<FString>>).OptionsSource(&ProfileOptions)
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> S)->TSharedRef<SWidget>{return Text(S.IsValid()?*S:TEXT(""));})
        .OnSelectionChanged_Lambda([W](TSharedPtr<FString> S,ESelectInfo::Type How){if(How!=ESelectInfo::Direct&&W.IsValid()&&S.IsValid()){if(!W->bRunning&&!W->bCenteringCamera){if(W->bAutoSavePending){W->SaveCalibration();W->bAutoSavePending=false;}W->SelectProfile(FName(**S));}}})
        [SNew(STextBlock).Font(Font(17)).Text_Lambda([W](){return FText::FromName(W.IsValid()?W->SelectedProfile:NAME_None);})]];
    V->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SEditableTextBox).Font(Font(17)).HintText(FText::FromString(TEXT("新名称 / 导入文件名（不含.json）"))).OnTextChanged_Lambda([W](const FText& T){if(W.IsValid())W->NewProfileName=T.ToString();})];
    auto Row=[W](const TCHAR* A,int32 AI,const TCHAR* B,int32 BI)->TSharedRef<SWidget>
    {return SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[Button(A,[W,AI](){if(W.IsValid())W->ManageProfile(AI);})]+SHorizontalBox::Slot().FillWidth(1)[Button(B,[W,BI](){if(W.IsValid())W->ManageProfile(BI);})];};
    V->AddSlot().AutoHeight().Padding(0,0,0,10)[Row(TEXT("新建默认配置"),0,TEXT("复制当前配置"),2)];
    V->AddSlot().AutoHeight().Padding(0,0,0,10)[Row(TEXT("重命名所选配置"),3,TEXT("删除所选 · 点两次确认"),4)];
    V->AddSlot().AutoHeight().Padding(0,0,0,14)[Row(TEXT("导出所选 JSON"),5,TEXT("导入 JSON 为新配置"),6)];
    V->AddSlot().AutoHeight()[Text(TEXT("JSON 交换目录：Saved/VRMFaceNodes/Profiles\n导入：把文件放入上述目录，填写文件名，再点击导入。已有同名配置不会被覆盖。\n导出内容只有参数，不包含人脸视频和录音。"),14)];
    return SNew(SBox).IsEnabled_Lambda([W](){return W.IsValid()&&!W->bRunning;})[V];
}

TSharedRef<SWidget> AVRMFaceCalibrationGuide::BuildConfigurationContent()
{
    const TWeakObjectPtr<AVRMFaceCalibrationGuide> W(this);
    auto Panel=SNew(SVerticalBox);
    Panel->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(STextBlock).Font(Font(25)).ColorAndOpacity(Accent).Text(FText::FromString(TEXT("面捕设置")))];
    auto Body=SNew(SVerticalBox);
    Body->AddSlot().AutoHeight()[BuildCalibrationPage()];
    Body->AddSlot().AutoHeight().Padding(0,8,0,16)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[Text(TEXT("面部微调"),17)].BodyContent()[BuildTweaksPage()]];
    Body->AddSlot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[Text(TEXT("配置管理"),15)].BodyContent()[BuildProfilesPage()]];
    Panel->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Body]];
    Panel->AddSlot().AutoHeight().Padding(0,14,0,8)[SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Accent).AutoWrapText(true).Text_Lambda([W](){return FText::FromString(W.IsValid()?W->Status:TEXT(""));})];
    Panel->AddSlot().AutoHeight()[SNew(STextBlock).Font(Font(13)).ColorAndOpacity(Muted).AutoWrapText(true).Text_Lambda([W](){return FText::FromString(W.IsValid()?FString(W->bFreshInput?TEXT("面捕数据更新中"):TEXT("等待面捕或回放"))+TEXT(" · ")+(W->StoredSlot.IsNone()?W->ProfileSlot:W->StoredSlot).ToString():TEXT(""));})];
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(22).BorderBackgroundColor(FLinearColor(.018f,.035f,.05f,1.f))[Panel];
}

void AVRMFaceCalibrationGuide::OpenConfigurationWindow()
{
    if(!bShowInterface || !GetWorld() || !GetWorld()->IsGameWorld() || IsRunningCommandlet() || !FApp::CanEverRender()
        || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().CanDisplayWindows())return;
    bCalibrationWindowPaused=false;
    if(!ConfigurationWindow.IsValid())
    {
        ConfigurationWindow=SNew(SWindow).Title(FText::FromString(TEXT("面捕设置")))
            .ClientSize(FVector2D(580,620)).MinWidth(480).MinHeight(480).SizingRule(ESizingRule::UserSized)
            .SupportsMaximize(true).SupportsMinimize(true).IsTopmostWindow(false)[BuildConfigurationContent()];
        // Keep the widget tree, pending edits, scroll position and window size when closed.
        ConfigurationWindow->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateWeakLambda(this,
            [this](const TSharedRef<SWindow>&){CloseConfigurationWindow();}));
        FSlateApplication::Get().ReleaseAllPointerCapture();
        FSlateApplication::Get().AddWindow(ConfigurationWindow.ToSharedRef());
    }
    else
    {
        FSlateApplication::Get().ReleaseAllPointerCapture();
        if(ConfigurationWindow->IsWindowMinimized())ConfigurationWindow->Restore();
        ConfigurationWindow->ShowWindow();ConfigurationWindow->BringToFront(true);
    }
    bPanelOpen=true;
}

void AVRMFaceCalibrationGuide::CloseConfigurationWindow(bool bDestroy)
{
    bPanelOpen=false;
    if(!bDestroy && bRunning)
    {
        bCalibrationWindowPaused=true;PendingSampleSeconds=0.f;bHasMotionHead=false;
        Status=TEXT("窗口已关闭，校准暂停。从工具菜单重新打开后继续。");
    }
    if(ConfigurationWindow.IsValid())
    {
        ConfigurationWindow->HideWindow();
        if(bDestroy)
        {
            // Detach option lists before the Actor/world can be collected. Native destruction is deferred.
            ConfigurationWindow->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride());
            ConfigurationWindow->SetContent(SNullWidget::NullWidget);
            ProfilePicker.Reset();MorphPicker.Reset();
            if(FSlateApplication::IsInitialized())ConfigurationWindow->RequestDestroyWindow();
            ConfigurationWindow.Reset();
        }
    }
}

void AVRMFaceCalibrationGuide::TogglePanel()
{
    if(bPanelOpen)CloseConfigurationWindow();else OpenConfigurationWindow();
}

void AVRMFaceCalibrationGuide::BuildInterface()
{
    auto* View=GetWorld()?GetWorld()->GetGameViewport():nullptr;if(!View || !bComparisonLabels)return;
    const TWeakObjectPtr<AVRMFaceCalibrationGuide> W(this);
    auto Label=[&](const FString& Title,const FString& Subtitle,const FString& Note)->TSharedRef<SWidget>
    {return SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly).VAlign(VAlign_Top)[SNew(SBox).WidthOverride(420)[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(24)).ColorAndOpacity(Accent).Justification(ETextJustify::Center).WrapTextAt(400).Text(FText::FromString(Title))]
        +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(STextBlock).Font(Font(16)).Justification(ETextJustify::Center).WrapTextAt(400).Text(FText::FromString(Subtitle))]
        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(13)).ColorAndOpacity(Muted).Justification(ETextJustify::Center).WrapTextAt(400).Text(FText::FromString(Note))]]];};

    auto Columns=SNew(SHorizontalBox);
    Columns->AddSlot().FillWidth(1)[Label(TEXT("古法手搓"),TEXT("你的原始手调映射"),TEXT(""))];
    Columns->AddSlot().FillWidth(1)[Label(TEXT("VRM4U + AI配置插件"),TEXT("GPT-Astra制作的快速映射"),TEXT("手调思路 · 自适应校准 · 下巴联动"))];
    if(bThreeWayComparison)Columns->AddSlot().FillWidth(1)[Label(TEXT("VRM4U 原始面捕"),TEXT("直接导入 · 无面部增强"),TEXT(""))];
    Interface=SNew(SOverlay)
        +SOverlay::Slot().VAlign(VAlign_Top).Padding(12,16)[Columns]
        +SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Center).Padding(12)[SNew(STextBlock).Font(Font(13)).ColorAndOpacity(Muted)
            .Text_Lambda([W](){return FText::FromString(W.IsValid()?(W->bFreshInput?TEXT("同一主题 · 同步驱动 · 头发已隐藏"):TEXT("等待面捕 / 回放 · 三组使用同一主题")):TEXT(""));})];
    View->AddViewportWidgetContent(Interface.ToSharedRef(),20);
}
