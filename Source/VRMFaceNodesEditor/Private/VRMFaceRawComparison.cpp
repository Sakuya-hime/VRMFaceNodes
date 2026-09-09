#include "VRMFaceEditorLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/PoseAsset.h"
#include "AnimGraphNode_LiveLinkPose.h"
#include "LiveLinkRemapAsset.h"
#include "AnimGraphNode_PoseBlendNode.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "Factories/AnimBlueprintFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "Misc/PackageName.h"

UAnimBlueprint* UVRMFaceEditorLibrary::CreateRawComparison(USkeletalMesh* Model,UPoseAsset* ImportedPose,const FString& Folder,FName Subject)
{
    if(!Model||!ImportedPose||!Model->GetSkeleton()||!Folder.StartsWith(TEXT("/Game/")))return nullptr;
    auto& Tools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    FString Package,Name;Tools.CreateUniqueAssetName(Folder/TEXT("ABP_VRM4U_Raw"),TEXT(""),Package,Name);
    auto* Factory=NewObject<UAnimBlueprintFactory>();Factory->TargetSkeleton=Model->GetSkeleton();Factory->PreviewSkeletalMesh=Model;Factory->ParentClass=UAnimInstance::StaticClass();
    auto* BP=Cast<UAnimBlueprint>(Tools.CreateAsset(Name,Folder,UAnimBlueprint::StaticClass(),Factory));if(!BP)return nullptr;
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UEdGraph* G=nullptr;
    for(auto* Graph:Graphs)if(Graph->GetFName()==TEXT("AnimGraph"))G=Graph;if(!G)return nullptr;
    UAnimGraphNode_Root* Root=nullptr;for(UEdGraphNode* N:G->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(N))Root=R;if(!Root)return nullptr;
    FGraphNodeCreator<UAnimGraphNode_LocalRefPose> RC(*G);auto* Ref=RC.CreateNode();Ref->NodePosX=-640;RC.Finalize();
    FGraphNodeCreator<UAnimGraphNode_LiveLinkPose> LC(*G);auto* Live=LC.CreateNode();Live->NodePosX=-340;Live->Node.LiveLinkSubjectName=Subject;Live->Node.RetargetAsset=ULiveLinkRemapAsset::StaticClass();LC.Finalize();
    FGraphNodeCreator<UAnimGraphNode_PoseBlendNode> PC(*G);auto* Pose=PC.CreateNode();Pose->NodePosX=0;Pose->Node.PoseAsset=ImportedPose;PC.Finalize();
    Root->NodePosX=430;
    auto Output=[](UEdGraphNode* N){for(auto* P:N->Pins)if(P->Direction==EGPD_Output&&P->PinType.PinCategory==TEXT("struct"))return P;return static_cast<UEdGraphPin*>(nullptr);};
    UEdGraphNode* Base=Ref;
    if(Model->GetRefSkeleton().FindBoneIndex(TEXT("J_Bip_L_UpperArm"))!=INDEX_NONE && Model->GetRefSkeleton().FindBoneIndex(TEXT("J_Bip_R_UpperArm"))!=INDEX_NONE)
    {
        Ref->NodePosX=-1650;
        FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> AC(*G);auto* CS=AC.CreateNode();CS->NodePosX=-1400;AC.Finalize();
        if(!G->GetSchema()->TryCreateConnection(Output(Ref),CS->FindPin(TEXT("LocalPose"))))return nullptr;
        Base=CS;
        for(int32 I=0;I<2;++I)
        {
            FGraphNodeCreator<UAnimGraphNode_ModifyBone> BC(*G);auto* Bone=BC.CreateNode();Bone->NodePosX=-1160+I*260;
            Bone->Node.BoneToModify.BoneName=I==0?TEXT("J_Bip_L_UpperArm"):TEXT("J_Bip_R_UpperArm");
            Bone->Node.Rotation=FRotator(I==0?-70:70,0,0);Bone->Node.RotationMode=BMM_Additive;Bone->Node.RotationSpace=BCS_ComponentSpace;BC.Finalize();
            if(!G->GetSchema()->TryCreateConnection(Output(Base),Bone->FindPin(TEXT("ComponentPose"))))return nullptr;Base=Bone;
        }
        FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> DC(*G);auto* Local=DC.CreateNode();Local->NodePosX=-620;DC.Finalize();
        if(!G->GetSchema()->TryCreateConnection(Output(Base),Local->FindPin(TEXT("ComponentPose"))))return nullptr;Base=Local;
    }
    if(!G->GetSchema()->TryCreateConnection(Output(Base),Live->FindPin(TEXT("InputPose")))||!G->GetSchema()->TryCreateConnection(Output(Live),Pose->FindPin(TEXT("SourcePose")))||!G->GetSchema()->TryCreateConnection(Output(Pose),Root->FindPin(TEXT("Result"))))return nullptr;
    FGraphNodeCreator<UEdGraphNode_Comment> CC(*G);auto* C=CC.CreateNode();C->NodePosX=-640;C->NodePosY=-170;C->NodeWidth=1420;C->NodeHeight=120;CC.Finalize();
    C->NodeComment=TEXT("VRM4U 原始对照：Live Link 原始曲线 → VRM4U 导入生成的表情姿势 → 输出。\n前方只放松手臂；面部不经过个人校准、防抖、牙齿避让或下巴联动。");
    FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status==BS_UpToDate?BP:nullptr;
}
