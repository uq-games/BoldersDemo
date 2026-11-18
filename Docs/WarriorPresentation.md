# 战士悬停轮廓与移动反馈

## 参考与素材

- 悬停逻辑参考只读项目 `G:/UE5/Learn-UE5-GAS-RPG`，不是最初指定的 Nekos496 项目：`AuraPlayerController::CursorTrace` 比较前后命中对象，`AuraEnemy::HighlightActor/UnHighlightActor` 对角色和武器切换 Custom Depth。课程中的 Highlight 是函数名，表现是描边。最初仅对 AssignedCharacter 实现本地黄色轮廓；现已扩展为下述怪物悬浮与选中反馈。
- `M_HoverOutline` 为本项目新建后处理材质，Stencil 200 黄色 / 201 红色外轮廓、2 像素邻域，并检查遮挡；没有复制参考项目的 GAS、接口或角色架构。TestMap 中 `HoverOutline` 无边界后处理体积引用它；渲染配置 `r.CustomDepth=3`。
- 2026-11-17 修正描边合成顺序：从 `Scene Color After Tonemapping` 移到 `Scene Color Before DOF`，使 CustomDepth/Stencil 的逐帧采样偏移与场景一起经过 TSR/TAA，避免将未稳定的轮廓叠加到已经稳定的画面上。UE 5.9 的 `Before Bloom` 仍位于时间上采样之后，不适用于此修复。保留默认 CustomDepth jitter，使其与用于遮挡判断的 SceneDepth 对齐。
- 本次只读对照 `G:/UE5/Nekos496_learn_cpp`：C++ 中未找到鼠标悬停 CustomDepth 实现；`CiciToonCharacterShaderPak/Stage/Materials/M_Outline` 与 `BackGroundMMD/Materials/M_AvaterOutline` 资产包含 Masked / WorldPositionOffset 外扩描边设置，属于不同的材质方案，没有直接移植。修复保留当前本地悬停、弱引用清理、武器描边和控制权限逻辑。
- `Scripts/Editor/fix_hover_outline.py` 仅修复并保存现有描边材质；完整表现生成脚本也同步使用 Before DOF，避免重建时复发。原材质备份位于 `Saved/HoverOutlineFix/M_HoverOutline.before.uasset`。
- 本次验证：UE 5.9 开启渲染的命令行材质编译/保存通过（0 错误、0 警告）；独立进程重新加载材质及 TestMap，确认 Before DOF 已持久化且无边界后处理体积仍引用该材质。日志在 `Saved/HoverOutlineFix/Fix.log`、`Verify.log`。地图加载存在原有 GameplayCue 搜索路径与导航提示；未进行 PIE 鼠标悬停的连续画面验收。
- `T_PlayerMarkers` 使用 GPT Image 第一版完整图片，包含原图的 1P/2P 文字与倒三角。HUD 通过图集 UV 分别显示蓝、金标记，不再叠加运行时字体。原图与空白备选版本保留在 Assets/UI/PlayerMarkers/Source。
- Cinzel 字体来自 Google Fonts： https://github.com/google/fonts/tree/main/ofl/cinzel 。源字体与 SIL Open Font License 位于 Assets/UI/Fonts/Source，字体资产保留供其他 UI 使用，当前头顶标记不再引用它。
- 点击反馈采用本机 UE 5.9 官方 TP_TopDownBP 模板的 `FX_Cursor_Success` / `FX_Cursor_Failure`。通过临时模板项目的 AssetTools.migrate_packages 导入依赖，保留 `/Game/TopDown/Cursor` 官方目录。项目自有 HUD、轮廓和标记在 BoldersDemo 目录。
- 官方模板说明：https://dev.epicgames.com/documentation/en-us/unreal-engine/top-down-template-in-unreal-engine 。只保留上述官方 Niagara 短时点击特效，不绘制沿途路径点或持续终点标记。

## 代码分工

`ABoldersPlayerController::PlayerTick` 仅在本地检测鼠标，使用弱指针记录 HoveredCharacter。命中对象改变、鼠标离开、分配角色改变或控制器退出时关闭旧轮廓。`ABaseCharacter::SetHoverOutline` 同时遍历角色的网格组件，包括运行时创建的剑盾；胶囊不参与描边。

服务器在玩家进入时分配 PlayerSlot，监听服务器主机为 1P，加入的玩家为 2P。职业选择界面先显示编号。确认后，角色沿用该编号，并在对应的出生点生成。服务器把角色编号复制给所有客户端，控制权限仍由 PlayerController.AssignedCharacter 判断。

`ABoldersHUD` 只负责头顶标记。`BP_BoldersHUD` 配置图集和大小，由 BP_BoldersGameMode.HUDClass 指定。MarkerAtlas 使用 UPROPERTY 持有。

移动由服务器 AIController 寻路执行。客户端不接收用于绘制的路径数组；已移除路径预览定时器、ClientUpdateMovePreview RPC 和 HUD 路径绘制。相机仍在本地处理。

点击反馈在服务器校验并调用 MoveToLocation 后返回：成功显示官方成功特效，合法控制者的移动请求被拒绝显示失败特效。角色选择、路径绘制和黄色描边是独立状态。

## 使用

### 怪物描边（2026-11-17）

- 本地玩家的存活敌对 `AEnemyCharacter` 支持悬浮黄色（Stencil 200）、选中红色（Stencil 201）。点击怪物即可选中，不要求先选战士；只有已选自己的战士时才沿用原有攻击请求。
- 红色优先于黄色，鼠标离开后保留。点击另一只怪物切换选中，点击地面、空白或其他角色取消怪物选中。取消后若仍悬浮该怪物，恢复黄色。
- `SelectedEnemy` 是本地弱引用，与用于发出指令的 `SelectedCharacter` 分开；不复制、不改变 Actor Owner 或服务器攻击校验。死亡、销毁、己方角色死亡、重新分配角色和控制器退出时清理状态。
- 角色本体和所属网格组件共用颜色。后处理材质继续在 Before DOF 合成，保留遮挡检查；红黄同时出现时，重叠外沿优先显示红色。
- `Scripts/Editor/setup_outline_material.py` 是独立材质生成入口，完整表现生成脚本复用它。修改前材质备份：`Saved/MonsterOutline/M_HoverOutline.before.uasset`。
- 验证：Development Editor 编译通过；材质编译保存通过（缓存服务有连接警告，无材质编译错误）；`BoldersDemo.Presentation.MonsterOutline` 自动化测试通过，覆盖黄色悬浮、红色优先、移开保持、切换/取消、死亡/销毁及重新分配角色清理。日志为 `Saved/MonsterOutline/Material.log`、`Tests.log`。尚未进行实际鼠标操作的连续画面或双客户端视觉验收。

运行 TestMap，鼠标移到自己的战士显示黄色轮廓；左键选择自己的战士，再左键点地面移动。蓝色 1P 与金色 2P 对应不同玩家。另一玩家的战士不会被本地描边或选中。

编辑 BP_BoldersHUD 的 Markers 类别调整 MarkerSize、MarkerAtlas；编辑 BP_BoldersPlayerController 的 Presentation 类别调整成功／失败点击特效。`Scripts/Editor/setup_movement_presentation.py` 记录资产配置步骤。修改 GameMode 默认类配置后重新加载地图或编辑器，确保 PIE 使用新类。

完整实现见 Source/BoldersDemo 下 Player/BoldersPlayerController、Characters/BaseCharacter、Game/BoldersGameMode 和 UI/BoldersHUD 的 Public/Private 文件。此前 Docs/WarriorMovement.md 内代码是基础移动版本；本次新增表现以当前源文件为准。

## 验证（2026-11-16）

- 最终 Development Editor 编译成功，记录见 `Characters/Warrior/Presentation/Build.txt`。
- 重启编辑器后 HUD、GameMode 蓝图默认配置仍存在；两个 PIE 世界都显示一致的角色槽位 1、2。
- 真实客户端窗口检查：蓝金标记与字体可见；2P 鼠标悬停自己的角色出现黄色轮廓，移到 1P 时关闭；点地面后出现绕墙的黄色路径点与终点圈，到达后清除。
- 七项 RPC 移动回归全部通过：绕障、上下斜坡、上下台阶、拒绝高台阶、拒绝控制另一玩家角色。客户端与服务器位置误差小于 0.01 cm。完整采样在 `Characters/Warrior/Presentation/MovementRegression.json`。
- 中途更换目的地后停在新目标 45.81 cm 内；本地相机移动 1185.60 cm，服务器对应相机移动 0 cm。结果在 `RedirectAndCamera.json`。
- 最后仅调整 HUD 使用 Canvas 投影，重新编译并检查 1280×720 截图中标记位置；截图见 `Characters/Warrior/Presentation/Route.png`。
- 本次验证范围为 Editor 与同进程双人 PIE；未执行独立服务器、网络丢包或打包验证。

2026-11-16 标记修订：已切换到 GPT Image 第一版完整图片，删除 HUD 运行时字体叠加。重新编译成功，并在双人 PIE 中确认 1P/2P 原图文字与倒三角正确显示；截图见 Presentation/OriginalMarkers.png。

2026-11-16 简化修订：按用户最新要求删除路径缓存与自建 Niagara 路线方案（NS_CachedRoute、M_RouteDot、M_DestinationX、生成脚本和编辑器辅助代码），保留官方点击成功／失败特效、AI 寻路、黄色轮廓和原图头顶标记。上述路径截图与回归记录属于简化前的历史验证。

简化后验证：Development Editor 编译成功；HUD 与 PlayerController 蓝图编译正常；双人 PIE 的七项移动回归全部通过。已检查短时官方点击特效截图，双方 HUD 都没有 Niagara 组件。新证据为 Presentation/SimpleBuild.txt、SimpleClick.png、SimpleMovementRegression.json。
