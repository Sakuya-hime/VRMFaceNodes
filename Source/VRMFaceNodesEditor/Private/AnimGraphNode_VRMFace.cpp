#include "AnimGraphNode_VRMFace.h"
UAnimGraphNode_VRMFaceAutomatic::UAnimGraphNode_VRMFaceAutomatic() { Node.Stage=EVFNStage::Automatic; }
FText UAnimGraphNode_VRMFaceAutomatic::GetNodeTitle(ENodeTitleType::Type TitleType) const { return FText::FromString(TEXT("面捕 · 自动面捕")); }
FText UAnimGraphNode_VRMFaceAutomatic::GetTooltipText() const { return FText::FromString(TEXT("一个节点完成输入、映射、校准、防抖和头眼上身跟随。设置 Live Link 主题，骨骼和形态键自动识别。")); }
FText UAnimGraphNode_VRMFaceAutomatic::GetMenuCategory() const { return FText::FromString(TEXT("VRM 面捕")); }
UAnimGraphNode_VRMFaceInput::UAnimGraphNode_VRMFaceInput() { Node.Stage=EVFNStage::Input; }
FText UAnimGraphNode_VRMFaceInput::GetNodeTitle(ENodeTitleType::Type TitleType) const { return FText::FromString(TEXT("面捕 · 输入与自动映射")); }
FText UAnimGraphNode_VRMFaceInput::GetTooltipText() const { return FText::FromString(TEXT("把 ARKit 或 MetaHuman 曲线统一为标准表情，再匹配当前模型的形态键。")); }
FText UAnimGraphNode_VRMFaceInput::GetMenuCategory() const { return FText::FromString(TEXT("VRM 面捕")); }
UAnimGraphNode_VRMFaceCalibration::UAnimGraphNode_VRMFaceCalibration() { Node.Stage=EVFNStage::Calibration; }
FText UAnimGraphNode_VRMFaceCalibration::GetNodeTitle(ENodeTitleType::Type TitleType) const { return FText::FromString(TEXT("面捕 · 表情校准")); }
FText UAnimGraphNode_VRMFaceCalibration::GetTooltipText() const { return FText::FromString(TEXT("独立校准完整闭眼、嘬嘴上限、闭嘴修正及上唇内卷。")); }
FText UAnimGraphNode_VRMFaceCalibration::GetMenuCategory() const { return FText::FromString(TEXT("VRM 面捕")); }
UAnimGraphNode_VRMFaceTeeth::UAnimGraphNode_VRMFaceTeeth() { Node.Stage=EVFNStage::Teeth; }
FText UAnimGraphNode_VRMFaceTeeth::GetNodeTitle(ENodeTitleType::Type TitleType) const { return FText::FromString(TEXT("面捕 · 牙齿避让")); }
FText UAnimGraphNode_VRMFaceTeeth::GetTooltipText() const { return FText::FromString(TEXT("大幅歪嘴时驱动已经存在的牙齿与口腔避让形态键；不改变歪嘴幅度。")); }
FText UAnimGraphNode_VRMFaceTeeth::GetMenuCategory() const { return FText::FromString(TEXT("VRM 面捕")); }
UAnimGraphNode_VRMFaceStabilization::UAnimGraphNode_VRMFaceStabilization() { Node.Stage=EVFNStage::Stabilization; }
FText UAnimGraphNode_VRMFaceStabilization::GetNodeTitle(ENodeTitleType::Type TitleType) const { return FText::FromString(TEXT("面捕 · 晃头防抖")); }
FText UAnimGraphNode_VRMFaceStabilization::GetTooltipText() const { return FText::FromString(TEXT("根据头部角速度增强嘴部、眉毛和眼睑的时间平滑；主动闭眼快速响应。")); }
FText UAnimGraphNode_VRMFaceStabilization::GetMenuCategory() const { return FText::FromString(TEXT("VRM 面捕")); }
UAnimGraphNode_VRMFaceHeadBody::UAnimGraphNode_VRMFaceHeadBody() { Node.Stage=EVFNStage::HeadBody; }
FText UAnimGraphNode_VRMFaceHeadBody::GetNodeTitle(ENodeTitleType::Type TitleType) const { return FText::FromString(TEXT("面捕 · 头部与坐播跟随")); }
FText UAnimGraphNode_VRMFaceHeadBody::GetTooltipText() const { return FText::FromString(TEXT("头部旋转和平滑、较慢的上身跟随；可开启 VRoid 手臂放松。")); }
FText UAnimGraphNode_VRMFaceHeadBody::GetMenuCategory() const { return FText::FromString(TEXT("VRM 面捕")); }
UAnimGraphNode_VRMFaceEyes::UAnimGraphNode_VRMFaceEyes() { Node.Stage=EVFNStage::Eyes; }
FText UAnimGraphNode_VRMFaceEyes::GetNodeTitle(ENodeTitleType::Type TitleType) const { return FText::FromString(TEXT("面捕 · 八向眼球")); }
FText UAnimGraphNode_VRMFaceEyes::GetTooltipText() const { return FText::FromString(TEXT("依据上下、内外四组曲线组合出八向眼球旋转。")); }
FText UAnimGraphNode_VRMFaceEyes::GetMenuCategory() const { return FText::FromString(TEXT("VRM 面捕")); }
