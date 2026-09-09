#include "VRMFaceCalibration.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"

namespace
{
    FString Disk(FName N) {return TEXT("VRMFaceNodes_")+N.ToString();}
    const FString Catalog=TEXT("VRMFaceNodes_ProfileCatalog");
    FString ExportPath(FName N) {return FPaths::ProjectSavedDir()/TEXT("VRMFaceNodes/Profiles")/(N.ToString()+TEXT(".json"));}
}
void UVFNCalibrationSubsystem::LoadCatalog()
{
    if(bCatalogLoaded) return;bCatalogLoaded=true;
    if(!UGameplayStatics::DoesSaveGameExist(Catalog,0))return;
    if(auto* S=Cast<UVFNProfileCatalogSave>(UGameplayStatics::LoadGameFromSlot(Catalog,0)))
        if(S->Bindings.Num()<=256) for(const auto& P:S->Bindings)
            if(UVFNCalibrationLibrary::IsValidSlot(P.Key) && UVFNCalibrationLibrary::IsValidSlot(P.Value)) Bindings.Add(P.Key,P.Value);
}
bool UVFNCalibrationSubsystem::SaveCatalog()
{
    auto* S=Cast<UVFNProfileCatalogSave>(UGameplayStatics::CreateSaveGameObject(UVFNProfileCatalogSave::StaticClass()));
    S->Bindings=Bindings;return UGameplayStatics::SaveGameToSlot(S,Catalog,0);
}
FName UVFNCalibrationSubsystem::GetActiveSlot(FName Binding)
{
    LoadCatalog();if(const auto* N=Bindings.Find(Binding)) return *N;return Binding;
}
TArray<FName> UVFNCalibrationSubsystem::ListProfiles() const
{
    TArray<FString> Files;TArray<FName> Result;
    IFileManager::Get().FindFiles(Files,*(FPaths::ProjectSavedDir()/TEXT("SaveGames/VRMFaceNodes_*.sav")),true,false);
    for(const auto& F:Files)
    {const FName N(*FPaths::GetBaseFilename(F).RightChop(13));if(UVFNCalibrationLibrary::IsValidSlot(N))Result.AddUnique(N);}
    Result.Sort(FNameLexicalLess());return Result;
}
bool UVFNCalibrationSubsystem::Activate(FName Binding,FName Stored,FString& M)
{
    if(!UVFNCalibrationLibrary::IsValidSlot(Binding) || !Reload(Stored,M)) return false;
    LoadCatalog();const auto Before=Bindings;Bindings.Add(Binding,Stored);
    if(!SaveCatalog()) {Bindings=Before;M=TEXT("切换配置未保存，仍使用原配置。");return false;}
    Previews.Remove(Binding);++Revision;M=TEXT("已切换到：")+Stored.ToString()+TEXT("。无需修改动画蓝图，下次运行保持此选择。");return true;
}
bool UVFNCalibrationSubsystem::CopyProfile(FName From,FName To,FString& M)
{
    if(!UVFNCalibrationLibrary::IsValidSlot(To) || UGameplayStatics::DoesSaveGameExist(Disk(To),0)) {M=TEXT("新名称不合法或已存在，请换一个名称。");return false;}
    if(!UVFNCalibrationLibrary::IsValidSlot(From))return false;
    auto* Original=Cast<UVFNCalibrationSave>(UGameplayStatics::LoadGameFromSlot(Disk(From),0));
    if(!Original || !UVFNCalibrationLibrary::ValidateProfile(Original->Profile,M)){M=TEXT("源配置读取失败。");return false;}
    return Save(To,Original->Profile,M);
}
bool UVFNCalibrationSubsystem::RenameProfile(FName From,FName To,FString& M)
{
    if(From==To) {M=TEXT("新旧名称相同。");return false;}
    if(!CopyProfile(From,To,M))return false;
    LoadCatalog();const auto Before=Bindings;
    // Old unaliased animation bindings continue to find the renamed file.
    for(auto& P:Bindings) if(P.Value==From) P.Value=To;
    if(!Bindings.Contains(From))Bindings.Add(From,To);
    if(!SaveCatalog()) {Bindings=Before;M=TEXT("已创建新副本，但切换记录写入失败，旧配置仍保留。");return false;}
    if(!UGameplayStatics::DeleteGameInSlot(Disk(From),0)) {Bindings=Before;SaveCatalog();M=TEXT("新副本已创建，旧文件删除失败；未完成重命名。");return false;}
    Profiles.Remove(From);Previews.Remove(From);Attempted.Remove(From);++Revision;M=TEXT("已重命名为：")+To.ToString();return true;
}
bool UVFNCalibrationSubsystem::DeleteProfile(FName Slot,FString& M)
{
    if(!UVFNCalibrationLibrary::IsValidSlot(Slot) || !UGameplayStatics::DoesSaveGameExist(Disk(Slot),0)) {M=TEXT("所选配置不存在。");return false;}
    LoadCatalog();const auto Before=Bindings;
    for(auto It=Bindings.CreateIterator();It;++It)if(It.Value()==Slot)It.RemoveCurrent();
    if(!SaveCatalog()) {Bindings=Before;M=TEXT("配置目录记录写入失败，未删除文件。");return false;}
    if(!UGameplayStatics::DeleteGameInSlot(Disk(Slot),0)) {Bindings=Before;SaveCatalog();M=TEXT("删除失败，保留当前配置。");return false;}
    Profiles.Remove(Slot);Previews.Empty();Attempted.Remove(Slot);++Revision;M=TEXT("已删除所选配置；受影响的绑定回到默认配置或默认参数。");return true;
}
bool UVFNCalibrationSubsystem::ExportProfile(FName Slot,FString& M)
{
    if(!UVFNCalibrationLibrary::IsValidSlot(Slot))return false;
    auto* Original=Cast<UVFNCalibrationSave>(UGameplayStatics::LoadGameFromSlot(Disk(Slot),0));
    if(!Original || !UVFNCalibrationLibrary::ValidateProfile(Original->Profile,M)){M=TEXT("源配置读取失败。");return false;}
    FString Text;if(!FJsonObjectConverter::UStructToJsonObjectString(Original->Profile,Text)) {M=TEXT("配置转换失败。");return false;}
    const FString Path=ExportPath(Slot);IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    if(!FFileHelper::SaveStringToFile(Text,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {M=TEXT("导出写入失败。");return false;}
    M=TEXT("已导出：")+FPaths::ConvertRelativePathToFull(Path);return true;
}
bool UVFNCalibrationSubsystem::ImportProfile(FName File,FName NewSlot,FString& M)
{
    if(!UVFNCalibrationLibrary::IsValidSlot(File) || !UVFNCalibrationLibrary::IsValidSlot(NewSlot) || UGameplayStatics::DoesSaveGameExist(Disk(NewSlot),0)) {M=TEXT("名称不合法或目标配置已存在；导入不会覆盖现有配置。");return false;}
    const FString Path=ExportPath(File);const int64 Size=IFileManager::Get().FileSize(*Path);
    if(Size<=0 || Size>1024*1024) {M=TEXT("请把 JSON 放入 Saved/VRMFaceNodes/Profiles，文件需小于 1 MB。");return false;}
    FString Text;FVFNPerformerProfile P;
    if(!FFileHelper::LoadFileToString(Text,*Path) || !FJsonObjectConverter::JsonObjectStringToUStruct(Text,&P,0,0) || !UVFNCalibrationLibrary::ValidateProfile(P,M)) {M=TEXT("JSON 格式、版本或参数校验失败，未导入。");return false;}
    return Save(NewSlot,P,M);
}
