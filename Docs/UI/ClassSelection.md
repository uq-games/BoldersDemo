# 开局职业选择

实现日期：2026-11-17。TestMap 使用现有 GameMode，开局先创建 CameraPawn，玩家确认职业后才生成正式角色。

## 编辑入口

- `/Game/BoldersDemo/Blueprints/UI/ClassSelection/WBP_ClassSelection`：主界面布局。
- 同目录 `WBP_ClassCard`：三个职业共用的卡片；`BP_CharacterPreview`：独立展示 Actor，可调整 Capture 镜头（当前 FOV 29）。
- `/Game/BoldersDemo/Assets/UI/ClassSelection/DA_ClassSelectionCatalog`：职业顺序、允许生成的职业、界面材质和贴图。
- 同目录 `DA_Class_Warrior`、`DA_Class_Archer`、`DA_Class_Mage`：职业名字、说明、正式角色类、展示模型、武器、石台、动作与动画蓝图。
- `/Game/BoldersDemo/Blueprints/Characters/<职业>/Preview/ABP_Preview_<职业>`：三个展示动作的混合。
- `/Game/BoldersDemo/Assets/Characters/<职业>/Animations/Preview`：展示动作副本。

展示角色继承 Actor，不继承 BaseCharacter，没有 ASC、AIController、寻路或攻击逻辑。正式角色仍使用原来的职业蓝图。弓弦仅作展示，不生成箭矢。

## 动作

| 职业 | A 常态循环 | B 鼠标悬停 | C 选中 |
| --- | --- | --- | --- |
| 战士 | GeneralIdle_A | Melee_Block | Cheering |
| 弓箭手 | Bow_Idle | Bow_Draw | Waving |
| 法师 | GeneralIdle_A | Magic_Spellcasting | Magic_Summon |

B、C 播放一次，然后平滑回到 A。选中标记不随动作结束而消失。选中优先于悬停；取消选择恢复未选中状态。动画蓝图使用 Blend Poses by Int，切换约 0.25 秒，重新进入动作时重置播放时间。

初始动作节点由编辑器命令 `Bolders.BuildClassSelection` 创建。已有动画图不会被这个命令覆盖；以后换动作时，应同时更新 Data Asset 的动画引用及对应 ABP 的 Sequence Player，保证播放长度和实际动画一致。

## 数据流与网络

```mermaid
flowchart TD
    A[本地 UMG 卡片] --> B[独立展示 Actor 和动画蓝图]
    B --> C[SceneCapture2D 和 RenderTarget]
    C --> D[UI 材质显示实时角色]
    A --> E[确认职业 ClassId]
    E --> F[PlayerController Server RPC]
    F --> G[GameMode 检查 Catalog 白名单]
    G --> H[服务器生成正式角色并分配 AIController]
    H --> I[AssignedCharacter 复制到所属客户端]
    I --> J[关闭界面并销毁本地展示 Actor]
```

客户端只提交 ClassId，不提交任意角色类或路径。服务器拒绝不在 Catalog 中的职业，已分配角色后也不会重复生成。两位玩家可以选相同职业。当前玩家槽位按确认顺序分配，最多使用地图已有的出生点。

预览 Actor、RenderTarget、动态材质都由本地界面创建并持有 UPROPERTY 引用。退出界面销毁 Actor，关闭 Capture；没有预览网络复制。选择阶段 UIOnly 输入，HUD 隐藏；确认成功恢复 GameAndUI。

## 美术来源与生成

GPT Image 分别生成背景、透明金色卡框、透明确认按钮底板，源图和提示词保存在 `ArtSources/UI/ClassSelection/`。文字、点击、悬停和选中状态由 UMG 管理，角色使用实时 3D 渲染。

石台引用 Inferno_World_Free 的 SM_Stone_001；人物动作来自项目已有角色和 KayKit。配置脚本 `Scripts/Editor/setup_class_selection.py` 导入图像、创建 Data Assets、绑定 GameMode/PlayerController；详细 UMG 和动画图初始化由 `Private/Editor/ClassSelectionSetup.cpp` 完成。

## 验证记录

- Development Editor C++ 编译通过；两个 UMG 蓝图编译通过。
- 重启编辑器后进行了双人 PIE；检查中文字体、三个实时模型、石台和武器构图。
- 实际鼠标操作确认弓箭手、法师后进入 TestMap；主机通过相同 Widget 事件选择战士。
- 客户端选择法师时，主机仍保持未选择；选中动作结束后 PoseIndex 返回待机而选择保持。
- 无效 ClassId 被拒绝并显示失败信息；取消选择可重选；角色已分配后重复提交不增加角色。
- 双方确认后，两端都存在法师和战士，预览 Actor 数量均为零，选择 Widget 已移除。
- 全屏双人 PIE 出现显存预算超出提示；未进行性能定标或 Cook/打包验证。

本次未提交 Git，保留此前工作区改动。
