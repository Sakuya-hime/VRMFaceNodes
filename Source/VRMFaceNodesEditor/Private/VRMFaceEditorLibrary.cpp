#include "VRMFaceEditorLibrary.h"
#include "VRMFaceLibrary.h"
#include "AnimGraphNode_VRMFace.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "Factories/AnimBlueprintFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"

namespace
{
    bool Save(UObject* Asset)
    {
        if (!Asset) return false;
        UPackage* Package=Asset->GetOutermost(); Asset->MarkPackageDirty();
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        return UPackage::SavePackage(Package,Asset,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    }
    FString UniqueName(IAssetTools& Tools, const FString& Folder, const FString& Base)
    {
        FString Package,Name;Tools.CreateUniqueAssetName(Folder/Base,TEXT(""),Package,Name);return Name;
    }
    UEdGraph* AnimGraph(UAnimBlueprint* BP)
    {
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        for (UEdGraph* Graph:Graphs) if (Graph->GetFName()==TEXT("AnimGraph")) return Graph;
        return nullptr;
    }
}

bool UVRMFaceEditorLibrary::AddVerifiedTeethCorrectives(USkeletalMesh* Model, FString& Message)
{
    if (!Model || !Model->GetImportedModel() || Model->GetImportedModel()->LODModels.Num()!=1 || (GEditor && GEditor->PlayWorld))
    { Message=TEXT("需要编辑器中具有完整单 LOD 源数据的模型，且不在运行模式。");return false; }
    if (Model->FindMorphTarget(TEXT("VRoid_TeethRetract")) && Model->FindMorphTarget(TEXT("VRoid_MouthInnerRetract")))
    {Message=TEXT("模型已经具有两项牙齿修正，无需重复添加。");return true;}
    const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("VRMFaceNodes"));
    FString Json;
    if (!Plugin || !FFileHelper::LoadFileToString(Json,*(Plugin->GetBaseDir()/TEXT("Resources/VerifiedMouthProfile.json"))))
    {Message=TEXT("未找到已验证牙齿配置。");return false;}
    TArray<TSharedPtr<FJsonValue>> Groups;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Groups)) {Message=TEXT("牙齿配置格式无效。");return false;}
    FMeshDescription* Description=Model->GetMeshDescription(0);
    if(!Description) {Message=TEXT("模型缺少可编辑源顶点，已跳过牙齿修正。");return false;}
    FSkeletalMeshAttributes Attributes(*Description);
    auto PositionsRef=Attributes.GetVertexPositions();
    TArray<int32> Selected[2];
    for (int32 G=0;G<Groups.Num();++G)
    {
        const auto Obj=Groups[G]->AsObject();
        const auto& IDs=Obj->GetArrayField(TEXT("vertices"));const auto& Positions=Obj->GetArrayField(TEXT("positions"));
        if (IDs.Num()!=Positions.Num()) {Message=TEXT("牙齿配置校验失败。");return false;}
        for(int32 I=0;I<IDs.Num();++I)
        {
            const int32 ID=IDs[I]->AsNumber(); const auto& XYZ=Positions[I]->AsArray();
            if (XYZ.Num()!=3 || !Description->Vertices().IsValid(FVertexID(ID))) {Message=TEXT("模型拓扑不符合已验证配置，已跳过牙齿修正。");return false;}
            FVector3f Expected(XYZ[0]->AsNumber(),XYZ[1]->AsNumber(),XYZ[2]->AsNumber());
            if (!PositionsRef[FVertexID(ID)].Equals(Expected,.0001f)) {Message=TEXT("模型顶点与已验证配置不一致，已跳过牙齿修正；表情映射仍可使用。");return false;}
            Selected[G<4?0:1].AddUnique(ID);
        }
    }
    // Validation of every selected point completes before any mesh mutation.
    Model->Modify();
    for(int32 I=0;I<2;++I)
    {
        FName Name=I==0?FName(TEXT("VRoid_TeethRetract")):FName(TEXT("VRoid_MouthInnerRetract"));
        if (Model->FindMorphTarget(Name)) continue;
        Model->ModifyMeshDescription(0);
        if(!Attributes.RegisterMorphTargetAttribute(Name,false)) {Message=TEXT("创建牙齿源形态键失败。");return false;}
        auto Deltas=Attributes.GetVertexMorphPositionDelta(Name);
        for(int32 ID:Selected[I]) Deltas[FVertexID(ID)]=FVector3f(0,-1,0);
        if(auto* Info=Model->GetLODInfo(0)) Info->ImportedMorphTargetSourceFilename.FindOrAdd(Name.ToString()).SetGeneratedByEngine(true);
        if (USkeleton* Skeleton=Model->GetSkeleton())
        {
            Skeleton->AddCurveMetaData(Name);
            if (FCurveMetaData* Meta=Skeleton->GetCurveMetaData(Name)) Meta->Type.bMorphtarget=true;
        }
    }
    Model->CommitMeshDescription(0);Model->PostEditChange();
    Message=TEXT("已在模型副本添加牙齿和口腔避让；权重 1 对应向口腔内后退 1 厘米。");
    return true;
}

FVFNSetupResult UVRMFaceEditorLibrary::ConfigureModel(USkeletalMesh* Source, const FString& Folder, FName Subject, bool Teeth)
{
    FVFNSetupResult R;
    if (!Source || !Source->GetSkeleton() || !Folder.StartsWith(TEXT("/Game/")) || Folder.Contains(TEXT("..")) || IsRunningCommandlet() || (GEditor && GEditor->PlayWorld))
    {R.Notes.Add(TEXT("请选择骨骼模型和 /Game/ 下的目标文件夹，并停止运行。"));return R;}
    R.Profile=UVRMFaceLibrary::InspectModel(Source);
    if(R.Profile.MatchedCount==0) {R.Notes.Add(TEXT("没有识别到可用的面部形态键。请用 VRM4U 标准导入窗口重新导入，并关闭 No MorphTarget；也可手动使用节点覆盖形态键名称。未生成空配置。"));return R;}
    auto& Tools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    // All generated assets have unique names; the source model and its manual mapping are not edited.
    USkeleton* Skeleton=Cast<USkeleton>(Tools.DuplicateAsset(UniqueName(Tools,Folder,TEXT("SKEL_FaceReady")),Folder,Source->GetSkeleton()));
    USkeletalMesh* Mesh=Cast<USkeletalMesh>(Tools.DuplicateAsset(UniqueName(Tools,Folder,TEXT("SK_FaceReady")),Folder,Source));
    if (!Skeleton || !Mesh) {R.Notes.Add(TEXT("创建模型副本失败。"));return R;}
    Mesh->SetSkeleton(Skeleton);R.Model=Mesh;
    if (Teeth) {FString Note;AddVerifiedTeethCorrectives(Mesh,Note);R.Notes.Add(Note);}
    {FString Note;AddVerifiedChinCorrective(Mesh,Note);R.Notes.Add(Note);}
    if (UClass* PostClass=Source->GetPostProcessAnimBlueprint())
    {
        if (auto* SourcePost=Cast<UAnimBlueprint>(PostClass->ClassGeneratedBy))
        {
            auto* Post=Cast<UAnimBlueprint>(Tools.DuplicateAsset(UniqueName(Tools,Folder,TEXT("ABP_FacePhysics")),Folder,SourcePost));
            if (Post)
            {
                Post->TargetSkeleton=Skeleton;Post->SetPreviewMesh(Mesh);
                TArray<UEdGraph*> Graphs;Post->GetAllGraphs(Graphs);
                for (UEdGraph* Graph:Graphs)
                {
                    TArray<UEdGraphNode*> Remove;
                    for (UEdGraphNode* Node:Graph->Nodes)
                    {
                        if (!Node->GetClass()->GetName().Contains(TEXT("VrmPoseBlend"))) continue;
                        UEdGraphPin* In=nullptr;UEdGraphPin* Out=nullptr;
                        for (UEdGraphPin* Pin:Node->Pins)
                        {
                            if (Pin->PinType.PinCategory!=TEXT("struct")) continue;
                            UObject* Type=Pin->PinType.PinSubCategoryObject.Get();
                            if (!Type || !Type->GetName().Contains(TEXT("PoseLink"))) continue;
                            if(Pin->Direction==EGPD_Input) In=Pin;else Out=Pin;
                        }
                        if (In && Out && In->LinkedTo.Num()==1)
                        {
                            auto* Up=In->LinkedTo[0];auto Destinations=Out->LinkedTo;Out->BreakAllPinLinks();
                            for(auto* Down:Destinations) Graph->GetSchema()->TryCreateConnection(Up,Down);
                            Remove.Add(Node);
                        }
                    }
                    for(auto* Node:Remove) FBlueprintEditorUtils::RemoveNode(Post,Node,true);
                }
                FKismetEditorUtilities::CompileBlueprint(Post);
                if(Post->Status!=BS_UpToDate) {R.Notes.Add(TEXT("物理蓝图副本未能编译；保留生成资产供检查。"));return R;}
                Mesh->SetPostProcessAnimBlueprint(Post->GeneratedClass.Get());R.PhysicsBlueprint=Post;
                if(!Save(Post)) {R.Notes.Add(TEXT("物理蓝图副本保存失败。"));return R;}
                R.Notes.Add(TEXT("保留导入模型的物理后处理副本，移除重复的 VRM 表情姿势混合。"));
            }
        }
    }
    auto* Factory=NewObject<UAnimBlueprintFactory>();Factory->TargetSkeleton=Skeleton;Factory->PreviewSkeletalMesh=Mesh;Factory->ParentClass=UAnimInstance::StaticClass();
    auto* BP=Cast<UAnimBlueprint>(Tools.CreateAsset(UniqueName(Tools,Folder,TEXT("ABP_AutoFace")),Folder,UAnimBlueprint::StaticClass(),Factory));
    if(!BP) {R.Notes.Add(TEXT("创建动画蓝图失败。"));return R;}
    UEdGraph* Graph=AnimGraph(BP);if(!Graph) {R.Notes.Add(TEXT("动画图不存在。"));return R;}
    UAnimGraphNode_Root* Root=nullptr;for(UEdGraphNode* N:Graph->Nodes)if(auto* P=Cast<UAnimGraphNode_Root>(N))Root=P;
    if(!Root) {R.Notes.Add(TEXT("动画输出节点不存在。"));return R;}
    FGraphNodeCreator<UAnimGraphNode_VRMFaceAutomatic> Creator(*Graph);auto* Node=Creator.CreateNode();
    Node->Node.Subject=Subject;Node->Node.PerformerProfileSlot=TEXT("坐播校准");Node->Node.Settings.bRelaxArms=true;Node->NodePosX=0;Node->NodePosY=0;Creator.Finalize();
    FGraphNodeCreator<UAnimGraphNode_LocalRefPose> RefCreator(*Graph);auto* Ref=RefCreator.CreateNode();
    Ref->NodePosX=-360;Ref->NodePosY=0;RefCreator.Finalize();
    Graph->GetSchema()->TryCreateConnection(Ref->FindPin(TEXT("Pose")),Node->FindPin(TEXT("SourcePose")));
    Root->NodePosX=520;Root->NodePosY=0;
    UEdGraphPin* Pose=nullptr;for(auto* Pin:Node->Pins)if(Pin->Direction==EGPD_Output)Pose=Pin;
    if(!Pose || !Graph->GetSchema()->TryCreateConnection(Pose,Root->FindPin(TEXT("Result")))) {R.Notes.Add(TEXT("连接动画输出失败。"));return R;}
    FGraphNodeCreator<UEdGraphNode_Comment> CommentCreator(*Graph);auto* Comment=CommentCreator.CreateNode();
    Comment->NodePosX=-80;Comment->NodePosY=-210;Comment->NodeWidth=1060;Comment->NodeHeight=140;Comment->CommentColor=FLinearColor(.025f,.13f,.17f);CommentCreator.Finalize();
    Comment->NodeComment=TEXT("自动面捕｜填写 Live Link 主题。运行后从「工具 → 面捕设置」操作：增强开关、五步校准、校正摄像头。\n接入已有身体动画时，将基础姿势接入节点，并按需要关闭手臂放松。独立节点请搜索「面捕」。");
    FKismetEditorUtilities::CompileBlueprint(BP);R.AnimationBlueprint=BP;
    if(BP->Status!=BS_UpToDate) {R.Notes.Add(TEXT("动画蓝图未能编译。"));return R;}
    const bool SavedSkeleton=Save(Skeleton),SavedMesh=Save(Mesh),SavedBP=Save(BP);
    if(!SavedSkeleton || !SavedMesh || !SavedBP) {R.Notes.Add(TEXT("生成资产未全部保存成功，请检查目标文件夹的写入权限。"));return R;}
    R.Profile=UVRMFaceLibrary::InspectModel(Mesh);R.bSuccess=true;R.Notes.Append(R.Profile.Notes);
    R.Notes.Add(TEXT("把结果模型放入场景，动画模式选择动画蓝图，再指定 ABP_AutoFace。原始模型未修改。"));
    return R;
}

