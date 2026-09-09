#pragma once
#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "AnimNode_VRMFace.h"
#include "AnimGraphNode_VRMFace.generated.h"
UCLASS()
class VRMFACENODESEDITOR_API UAnimGraphNode_VRMFaceAutomatic : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="设置") FAnimNode_VRMFace Node;
    UAnimGraphNode_VRMFaceAutomatic();
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(.025f,.34f,.42f); }
};
UCLASS()
class VRMFACENODESEDITOR_API UAnimGraphNode_VRMFaceInput : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="设置") FAnimNode_VRMFace Node;
    UAnimGraphNode_VRMFaceInput();
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(.025f,.34f,.42f); }
};
UCLASS()
class VRMFACENODESEDITOR_API UAnimGraphNode_VRMFaceCalibration : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="设置") FAnimNode_VRMFace Node;
    UAnimGraphNode_VRMFaceCalibration();
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(.025f,.34f,.42f); }
};
UCLASS()
class VRMFACENODESEDITOR_API UAnimGraphNode_VRMFaceTeeth : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="设置") FAnimNode_VRMFace Node;
    UAnimGraphNode_VRMFaceTeeth();
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(.025f,.34f,.42f); }
};
UCLASS()
class VRMFACENODESEDITOR_API UAnimGraphNode_VRMFaceStabilization : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="设置") FAnimNode_VRMFace Node;
    UAnimGraphNode_VRMFaceStabilization();
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(.025f,.34f,.42f); }
};
UCLASS()
class VRMFACENODESEDITOR_API UAnimGraphNode_VRMFaceHeadBody : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="设置") FAnimNode_VRMFace Node;
    UAnimGraphNode_VRMFaceHeadBody();
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(.025f,.34f,.42f); }
};
UCLASS()
class VRMFACENODESEDITOR_API UAnimGraphNode_VRMFaceEyes : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="设置") FAnimNode_VRMFace Node;
    UAnimGraphNode_VRMFaceEyes();
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(.025f,.34f,.42f); }
};
