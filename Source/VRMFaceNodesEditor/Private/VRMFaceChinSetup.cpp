#include "VRMFaceEditorLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"

bool UVRMFaceEditorLibrary::AddVerifiedChinCorrective(USkeletalMesh* Model,FString& Message)
{
    const FName Name(TEXT("VFN_ChinAssist"));
    if(!Model || (GEditor&&GEditor->PlayWorld)){Message=TEXT("停止运行后，在模型副本中创建下巴联动。");return false;}
    if(Model->FindMorphTarget(Name)){Message=TEXT("已存在可回退的下巴联动。");return true;}
    auto* Desc=Model->GetMeshDescription(0);
    if(!Desc){Message=TEXT("无可编辑顶点，保留原下巴。");return false;}
    FSkeletalMeshAttributes Attr(*Desc);auto Positions=Attr.GetVertexPositions();
    const TArray<FName> Names=Attr.GetMorphTargetNames();
    FName Current,Old;
    for(FName N:Names){if(N.ToString().Equals(TEXT("jawOpen"),ESearchCase::IgnoreCase))Current=N;if(N.ToString().Equals(TEXT("jawOpen_old"),ESearchCase::IgnoreCase))Old=N;}
    if(Current.IsNone()||Old.IsNone()){Message=TEXT("此模型没有已验证的新旧下颌形态，保留原下巴。");return false;}
    const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("VRMFaceNodes"));FString Json;
    if(!Plugin||!FFileHelper::LoadFileToString(Json,*(Plugin->GetBaseDir()/TEXT("Resources/VerifiedMouthProfile.json"))))return false;
    TArray<TSharedPtr<FJsonValue>> Groups;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Groups)||Groups.Num()!=7)return false;
    TSet<int32> LowerOral,UpperTeeth;
    for(int32 G=0;G<Groups.Num();++G)
    {
        const auto O=Groups[G]->AsObject();const auto& IDs=O->GetArrayField(TEXT("vertices"));const auto& XYZ=O->GetArrayField(TEXT("positions"));
        if(IDs.Num()!=XYZ.Num())return false;
        for(int32 I=0;I<IDs.Num();++I)
        {
            const int32 ID=IDs[I]->AsNumber();const auto& P=XYZ[I]->AsArray();
            if(P.Num()!=3||!Desc->Vertices().IsValid(FVertexID(ID))||!Positions[FVertexID(ID)].Equals(FVector3f(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()),.0001f))
            {Message=TEXT("模型几何与已验证版本不一致，保留原下巴。");return false;}
            if(G<2)UpperTeeth.Add(ID);else LowerOral.Add(ID);
        }
    }
    auto A=Attr.GetVertexMorphPositionDelta(Old),B=Attr.GetVertexMorphPositionDelta(Current);
    auto Materials=Attr.GetPolygonGroupMaterialSlotNames();TSet<int32> Skin;
    for(const FTriangleID T:Desc->Triangles().GetElementIDs())
    {
        const FName Mat=Materials[Desc->GetTrianglePolygonGroup(T)];
        if(!Mat.ToString().Contains(TEXT("Face_00_SKIN")))continue;
        for(FVertexID V:Desc->GetTriangleVertices(T))Skin.Add(V.GetValue());
    }
    if(Skin.Num()<500){Message=TEXT("没有找到已验证的脸部皮肤区域，保留原下巴。");return false;}
    const auto Smooth=[](float X){X=FMath::Clamp(X,0.f,1.f);return X*X*(3.f-2.f*X);};
    TMap<int32,FVector3f> Changes;
    for(FVertexID V:Desc->Vertices().GetElementIDs())
    {
        const int32 ID=V.GetValue();if(UpperTeeth.Contains(ID))continue;
        const FVector3f P=Positions[V];float Mask=0.f;
        if(Skin.Contains(ID))Mask=Smooth((136.2f-P.Z)/1.8f)*Smooth((7.f-FMath::Abs(P.X))/2.f)*Smooth((P.Y-1.f)/2.f);
        else if(LowerOral.Contains(ID))Mask=1.f;
        if(Mask<=0.f)continue;
        FVector3f D=(A[V]-B[V])*Mask;
        // Prevent an outlier from producing a large local displacement.
        if(D.ContainsNaN()||D.Size()>3.f){Message=TEXT("下颌变形超出已验证范围，保留原下巴。");return false;}
        if(D.SizeSquared()>1.e-8f)Changes.Add(ID,D);
    }
    if(Changes.Num()<50){Message=TEXT("下巴没有足够的可用变形，保留原效果。");return false;}
    Model->Modify();Model->ModifyMeshDescription(0);
    if(!Attr.RegisterMorphTargetAttribute(Name,false))return false;
    auto Out=Attr.GetVertexMorphPositionDelta(Name);
    for(const auto& P:Changes)Out[FVertexID(P.Key)]=P.Value;
    if(auto* Info=Model->GetLODInfo(0))Info->ImportedMorphTargetSourceFilename.FindOrAdd(Name.ToString()).SetGeneratedByEngine(true);
    if(auto* Skeleton=Model->GetSkeleton()){Skeleton->Modify();Skeleton->AddCurveMetaData(Name);if(auto* Meta=Skeleton->GetCurveMetaData(Name))Meta->Type.bMorphtarget=true;}
    Model->CommitMeshDescription(0);Model->PostEditChange();
    Message=FString::Printf(TEXT("已添加下巴联动（%d 个顶点）；上牙不动，强度设为0即恢复原下巴。"),Changes.Num());return true;
}
