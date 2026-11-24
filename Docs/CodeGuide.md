# BoldersDemo 代码阅读与整理笔记

## 先顺着功能读

### 火山铸造岛的完整流程

1. `L_VolcanoCombat` 是默认地图。选角后由 GameMode 为每个控制器分配并占有角色。`BoldersPlayerController` 处理固定镜头的 WASD、点选攻击和地面攻距。`BoldersCombatMovementComponent` 在前摇时限制直控加速度，包括服务端重放客户端移动的情形。
2. `VolcanoEncounter` 用服务器时间推进两波和骑士战，负责检查点回血、队友复活和结算快照。守卫预警由复制的攻击阶段和锁定落点呈现。骑士的动作、命中和团灭停机由 `StatueBossCharacter` 管理。
3. `CharacterCombatComponent` 在入队时校验目标、攻距和机关元素，`BasicAttackAbility` 在前摇前再确认枪口与目标，蒙太奇通知触发出手。掩体查询与投射物敌我过滤各由现有攻击模块处理。`VolcanoHazard` 管理机关响应，`BladeCoating.cpp` 管理战士蘸刀、附魔过期与命中消耗。
4. `BoldersHUD` 渲染小地图、技能、结算和读牌界面，`VolcanoWaySign` 管理场景路牌和可读距离。地图和路牌弹窗占用输入时，玩家控制器暂停处理战斗命令。

### 1. 从点击到角色移动

1. [BoldersPlayerController.cpp](../Source/BoldersDemo/Private/Player/BoldersPlayerController.cpp) 注册输入和处理玩家角色分配。点击进入 [BoldersPlayerControllerCommands.cpp](../Source/BoldersDemo/Private/Player/BoldersPlayerControllerCommands.cpp) 的 `HandleCommandClick`，按点击对象分流。
2. 点击地面后，`ServerRequestMove` 核对角色归属、地面标签和导航投影。通过后调用 [BaseCharacter.cpp](../Source/BoldersDemo/Private/Characters/BaseCharacter.cpp) 的 `RequestMoveToLocation`。
3. [BoldersAIController.cpp](../Source/BoldersDemo/Private/AI/BoldersAIController.cpp) 执行角色寻路。鼠标悬停、选中状态、描边和阵营圆环的更新可沿 [BoldersPlayerControllerSelection.cpp](../Source/BoldersDemo/Private/Player/BoldersPlayerControllerSelection.cpp) 阅读。

### 2. 从普攻到伤害和特效

1. 点击敌人触发 `ServerRequestAttack`。[CharacterCombatComponent](../Source/BoldersDemo/Private/Combat/CharacterCombatComponent.cpp) 管理武器、当前招式和攻击请求。招式的范围、伤害、动画与特效配置见 [AttackLoadout.h](../Source/BoldersDemo/Public/Combat/Data/AttackLoadout.h)。
2. [BasicAttackAbility](../Source/BoldersDemo/Private/Combat/Abilities/BasicAttackAbility.cpp) 在服务器启动。距离或攻击站位不合适时，由 [AttackPositioningComponent](../Source/BoldersDemo/Private/AI/Positioning/AttackPositioningComponent.cpp) 寻找位置。Montage 上的 [AnimNotify_AttackHit](../Source/BoldersDemo/Private/Combat/Abilities/AnimNotify_AttackHit.cpp) 发出命中事件，接下来进入近战、投射物或落雷分支。 GAS 同步旁观端的动作。远端操控者由服务器发送同一蒙太奇和起始位置，取消时按播放实例停止。客户端播放驱动姿势和特效，命中通知在服务器处理。
3. 近战和落雷见 [AttackResolution.cpp](../Source/BoldersDemo/Private/Combat/Attacks/AttackResolution.cpp)，飞行和碰撞见 [AttackProjectile.cpp](../Source/BoldersDemo/Private/Combat/Attacks/AttackProjectile.cpp)。范围查询、遮挡、落点和冲击力共用 [AttackQueries.cpp](../Source/BoldersDemo/Private/Combat/Attacks/AttackQueries.cpp)。
4. [ElementalHit.cpp](../Source/BoldersDemo/Private/Combat/Attacks/ElementalHit.cpp) 根据已有附着和这次的攻击元素查反应表。[BaseCharacterCombat.cpp](../Source/BoldersDemo/Private/Characters/BaseCharacterCombat.cpp) 负责提交 Gameplay Effect，以及受伤、冻结、死亡和受击位移的响应。GAS 的属性、抗性和效果定义见 [Combat/Effects](../Source/BoldersDemo/Private/Combat/Effects)。
5. [BaseCharacterPresentation.cpp](../Source/BoldersDemo/Private/Characters/BaseCharacterPresentation.cpp) 接收表现 RPC，[AttackPresentation.cpp](../Source/BoldersDemo/Private/Combat/Presentation/AttackPresentation.cpp) 根据招式配置播放预警、命中和投射物特效。刀光拖尾和飞行粒子也在 `Combat/Presentation`。

### 3. 从选职业到生成角色

从 [BoldersPlayerControllerClassSelection.cpp](../Source/BoldersDemo/Private/Player/BoldersPlayerControllerClassSelection.cpp) 看选人界面的打开、输入模式和服务器请求，再到 [BoldersGameMode.cpp](../Source/BoldersDemo/Private/Game/BoldersGameMode.cpp) 看出生点、职业验证和角色生成。职业卡片、展示模型及其预览动画都在 [UI/ClassSelection](../Source/BoldersDemo/Private/UI/ClassSelection)，进游戏后的血条、图标和移动标记可读 [UI/Combat](../Source/BoldersDemo/Private/UI/Combat)。

## 文件放置

| 目录 | 内容 |
| --- | --- |
| `Source/BoldersDemo/Public` | 模块提供的头文件 |
| `Source/BoldersDemo/Private` | C++ 实现、私有头文件和测试 |
| `Characters/Roles`、`Characters/Animation`、`Characters/Presentation` | 角色子类、动画实例、阵营圆环 |
| `Combat/Abilities`、`Combat/Attacks`、`Combat/Data` | 攻击技能的生命周期、命中与投射物、招式数据 |
| `Combat/Effects`、`Combat/Equipment`、`Combat/Presentation`、`Combat/Environment` | GAS 属性与效果、武器、战斗特效、受冲击场景物件 |
| `AI/Positioning/EQS` | 攻击站位的查询节点 |
| `Private/Editor`、`Private/Tests` | 编辑器命令和自动化测试，各自按功能再分目录 |
| `Scripts/Editor` | 角色动画、角色表现、战斗、UI、地图的 Unreal 编辑器脚本 |
| `Scripts/Blender/Maps` | 用 Blender 生成地图网格的脚本 |

同一类的实现有多个 `.cpp` 时，先看同名头文件和主实现文件，再按文件名追踪相应职责。角色的组件创建、ASC 初始化和委托解绑集中在 `BaseCharacter.cpp`。源码重排沿用原来的反射类名和模块名，蓝图继续通过原有的 `/Script/BoldersDemo` 类路径引用原生类。

元素攻击的编辑器入口集中在 [Scripts/Editor/Combat/Elemental](../Scripts/Editor/Combat/Elemental)。`elemental_assets.py` 提供资产加载、保存和材质创建，`concept_vfx_assets.py` 提供概念特效网格生成和招式表现字段更新。执行重建前可以查看 [元素攻击](Combat/ElementalAttacks.md)、[概念特效](Combat/CombatConceptVFX.md)和 [战士刀光](Combat/WarriorBladeTrail.md) 中的资源保存范围。

## 在编辑器里验证

使用 UE 5.9 构建 `BoldersDemoEditor Win64 Development`。在 Session Frontend 的 Automation 页搜索 `BoldersDemo.`，运行元素、投射物、机关、路径和角色表现测试。打开 `L_VolcanoCombat` 可在 `ServerRequestAttack_Implementation`、`UBasicAttackAbility::OnHitEvent` 和 `AttackResolution::ApplyHit` 跟踪直控战斗。
