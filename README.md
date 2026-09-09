# VRM Face Nodes · VRM 面捕节点

**木花开耶姬的手调映射，做成可复用的中文虚幻插件。**

基于 VRM4U 的模型导入与表情通道，用一颗动画节点接入 Live Link，再进行个人校准、镜像和稳定处理。日常从 **工具 → 面捕设置** 打开独立配置窗口。版本 **1.3.0**，开发与验证环境为 **Windows / Unreal Engine 5.8.2**。

[中文完整教程](docs/教程.md) · [功能与限制](docs/功能与限制.md) · [源码构建与验证](docs/构建与验证.md) · [作者主页](https://github.com/Sakuya-hime)

## 三步开始

1. 安装 [VRM4U](https://github.com/ruyo/VRM4U)，用它导入 VRM，**不要勾选 No MorphTarget**。本插件不会替代 VRM4U 的文件导入器。
2. 安装本插件，连接手机 Live Link Face，在 Hub 中确认主题有持续更新的表情数据。
3. **工具 → 面捕设置**：选模型、填相同主题名，点 **接入面捕并放入场景**。运行后重新打开此窗口，完成 **重新校准 · 五步**。

## 下载

[UE 5.8.2 / Win64 预编译安装包与发布说明](https://github.com/Sakuya-hime/VRMFaceNodes/releases/tag/v1.3.0)。其他引擎版本按下方源码方式安装。

## 安装源码

从 **Code → Download ZIP** 下载，解压后把仓库文件夹改名为 `VRMFaceNodes`，放入 `你的项目/Plugins/`。正确位置是 `Plugins/VRMFaceNodes/VRMFaceNodes.uplugin`。安装前关闭编辑器。

本仓库提供源码和手调映射蓝图，**不包含预编译 DLL**。使用 Visual Studio 的 C++ 游戏开发环境编译项目；纯蓝图项目请先添加一个空 C++ 类，或按[构建说明](docs/构建与验证.md)使用 BuildPlugin。其他 UE 版本可能需要适配，不能直接假定二进制兼容。

启用 Live Link、IK Rig；手机 MetaHuman 实时流程还需 **MetaHuman Live Link**。VRM4U 另外安装。

## 主要功能

| 功能 | 用途 |
|---|---|
| 自动接入 | 识别可用形态键，生成模型、骨架、动画蓝图副本 |
| 面部增强总开关 | 开启整套增强；关闭后保留基础映射和基础方向驱动 |
| 五步校准 | 自然表情、闭眼、普通张嘴、最大张嘴、抬眉，约 35 秒 |
| 校正摄像头 | 坐正看屏幕，约一秒记录头部与视线的正方向 |
| 前置镜像 | 左右表情、头部、视线统一镜像，点头方向保持自然 |
| 微调与配置 | 即时使用、自动保存；配置可新建、复制、改名、删除、JSON 交换 |
| 腿脚 IK 链修复 | 检查标准 UE → VRoid 的两侧腿链与脚趾链，生成修复副本 |

**模型差异说明：** 原演示中额外生成牙齿避让与下巴补偿，使用了作者模型专用的顶点校验配置。该配置含模型几何数据，**不随开源包发布**。其他模型默认保留原牙齿与下巴；已有兼容修正形态键的模型可由运行节点驱动。面捕映射、校准、镜像与 IK 链修复不依赖该配置。详见[功能与限制](docs/功能与限制.md)。

## 署名与鸣谢

- **手调映射、效果调试与方案：木花开耶姬。** `BP_ManualFacialRemap` 保留手调蓝图算法。
- 插件实现、辅助调试和教程制作：GPT-6 Astra，在木花开耶姬反馈与验收下完成。
- VRM4U：Haruyoshi Yamamoto / ruyo。MetaHuman → ARKit 通道表改编自 VRM4U，保留其 MIT 声明。
- **感谢 [王瑨Artist](https://space.bilibili.com/4123208)，我的虚幻引擎启蒙老师。** 王老师以有趣的案例讲解 UE 物理引擎与蓝图。感谢王老师在 VRM4U 更新的第一时间想到我，把新功能分享给我。

## 许可与内容范围

插件源码与作者手调蓝图使用 [MIT 许可证](LICENSE)，第三方来源见 [ThirdPartyNotices.md](ThirdPartyNotices.md)。Unreal Engine、VRM4U、Live Link Face 分别遵循各自条款。

仓库不含个人 VRM 模型、材质、录屏、音乐、个人校准存档或模型专用顶点配置。教程中的模型与音乐不属于本仓库的 MIT 授权范围。
