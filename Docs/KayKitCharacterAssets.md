# KayKit 角色武器与动画目录

整理日期：2026-11-17。项目角色根目录为 `/Game/BoldersDemo/Assets/Characters/`。

## 项目角色

| 角色 | 武器目录 | 动画分类 |
| --- | --- | --- |
| Warrior | `Warrior/Weapons`：单手剑、双手剑、盾牌及双手握柄适配模型 | `Animations/Combat/OneHanded`、`TwoHanded`、`Shield`；共用 `Idle`、`Locomotion`、`HitReactions`、`Death` |
| Archer | `Archer/Weapons`：弓、箭、箭束、箭袋 | `Animations/Combat/Bow`；共用 `Idle`、`Locomotion`、`HitReactions`、`Death` |
| Mage | `Mage/Weapons`：法杖、魔杖、开合魔法书 | `Animations/Combat/Magic`；共用 `Idle`、`Locomotion`、`HitReactions`、`Death` |

动画资产沿用已有名称，分类目录表达动作用途，避免重命名资产产生额外路径变更。战士现有 `BS_Warrior_Locomotion` 保留原位置，其三个动画引用已更新。

## 战士双手动作与装备

`Warrior/Animations/Combat/TwoHanded` 包含：

- `A_Warrior_2H_Idle`：双手待机。
- `A_Warrior_2H_Attack_Chop`：双手劈砍。
- `A_Warrior_2H_Attack_Slice`：双手横斩。
- `A_Warrior_2H_Attack_Stab`：双手突刺。

这四个动画由原包 Medium 的对应 2H 动作复制，使用相同骨骼并指定战士预览网格体。动画具有常量值为 1 的 `TwoHandedWeapon` 浮点曲线。

现有 `UKayKitEquipmentComponent` 在角色骨骼求值后读取该曲线：值大于 0.5 时使用双手剑并隐藏盾牌；切回普通单手动作时恢复单手剑、盾牌和原挂点偏移。此逻辑只负责外观，不改变伤害、权威、复制或投射物生成。它需要角色使用现有的装备组件；将动画拖到普通 SkeletalMeshActor 上不会自动添加组件。

双手动作握距比原剑柄长，因此新增 `Weapons/SM_sword_2handed_LongGrip`，仅延展原模型 Z<0 的握柄区域，保留刀身和原武器资产。运行时双手模式在 `Weapon_R` 下使用相对位置 `(-2,-8,75)`；单手模式恢复零偏移。未修改原包共享动画的动作数据。

## 原包动画

`/Game/KayKit_Adventurers` 的角色和 `Shared/Animations/Medium` 保留原归属，动画目录统一为：

- `Combat/OneHanded`、`TwoHanded`、`DualWield`、`Unarmed`、`Shield`、`Bow`、`Magic`、`RangedOneHanded`、`RangedTwoHanded`。
- `Locomotion`、`Locomotion/Jump`、`Locomotion/Dodge`。
- `Idle`、`HitReactions`、`Death`、`Spawn`、`Interactions`。
- `ReferencePoses`、`Emotes`、`Tools`、`Special`。

通过 Unreal 资产接口移动 187 个资产、修复重定向和引用，并通过编辑器接口删除 22 个空目录。没有删除原包剩余动画。

## 验证与维护

- 编辑器开发版本编译通过。
- 最终编译版本实际播放 40 个项目角色动画，在起始、中间和结束附近共检查 120 个采样点，装备切换、双手剑偏移、弓弦与无手持箭检查通过。结果见 `Saved/KayKitOrganize20261117/FinalRuntime.json`。
- 四个双手动作分别检查了正面和侧面的中段画面，验证使用的双手剑模型和偏移与最终组件一致。
- 加载、动画骨骼、动画时长、移动混合动画引用检查见 `Saved/KayKitOrganize20261117/Audit.json`。
- 运行时检查及四个双手动作正侧面画面保存在同目录；备份为 `Before.zip`。
- 整理同时更新了运行时装备加载路径和 `WarriorAnimationSetup.cpp` 的动画路径。
- 未新建蓝图，默认地图配置保持 `TestMap`。原包和项目角色之间的既有骨骼依赖保留。
