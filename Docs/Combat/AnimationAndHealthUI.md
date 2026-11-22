# 角色动画、单次攻击与血条

## 动画配置和所有权

`ABaseCharacter::AnimationBlueprintClass` 是 `TSubclassOf<UBaseCharacterAnimInstance>`，在角色蓝图默认值里选择动画蓝图生成类。不是保存编辑器的 `UAnimBlueprint` 资产对象，也不是手动 NewObject 一个实例。Mesh 创建并持有运行时 AnimInstance；`GetCharacterAnimation()` 每次从 Mesh 获取，避免替换网格体或动画类后缓存失效。OnConstruction 和 BeginPlay 只在配置类变化时应用，避免重复重建动画实例。

`UBaseCharacterAnimInstance` 在动画更新中读取角色速度与生命状态，暴露 `Speed`、`bDead`；它不负责扣血、网络命令或 AI。`UWarriorAnimInstance` 继承它，保留已有 ABP_Warrior 的资产类引用。

- 战士：原有 locomotion BlendSpace → DefaultSlot → 输出，保留攻击 Montage 的命中通知。
- 蘑菇：BS_Mushroom_Locomotion → DefaultSlot；bDead=false 使用该分支，bDead=true 使用非循环死亡序列。死亡分支优先于受击 Slot，动画播完保留末帧。
- 非致命伤害：服务器 GAS Health 变化回调触发可靠 Multicast，播放 AM_Mushroom_Hit。客户端不据此计算伤害。
- 致命伤害：服务器停止移动、取消攻击；同步的 Health=0 使各端动画实例进入死亡分支。死亡是持续状态，不依赖只广播一次的死亡 RPC。当前没有复活流程。

蘑菇源动作引用第三方 `MonsterForSurvivalGame/Animation/Polyart/Mushroom/SmileVersion` 下的 IdleNormal、walkFWD、GetHit、Die，未修改第三方资产。项目自有 BlendSpace 和 Montage 在 `Assets/Characters/EnemyMushroom/Animations`，动画蓝图在 `Blueprints/Characters/Enemy/ABP_EnemyMushroom`。

## 一条命令只攻击一次

接近与选点流程不变。`OnAttackMontageEnded` 清除 bAttacking、bHitPending 和目标，不再调用 StartAttackMontage。下次点击敌人才会产生下一次命令，三个攻击动作仍按次序选择。AnimNotify 仍只在服务器执行一次命中判定，距离或遮挡不满足时可以挥空，不保证每条命令都扣血。

## 血条

ABoldersHUD 将队伍血条显示在左上角，按 PlayerSlot 排序，通过本地 PlayerController 的 AssignedCharacter 标记 YOU/ALLY。敌方血条悬浮在角色上方，掉血后显示当前生命和上限，死亡后收起。

血量填充和数字直接读取角色 GAS AttributeSet 的 Health 和 MaxHealth。1P 使用青色，2P 使用暖金色，敌方为红色。扣血时，琥珀色残影先停留 0.23 秒，再于 0.42 秒内收至实际血量。残影仅是本地显示状态。

薄边血条和职业图标由 GPT Image 生成，原图在 `ArtSources/UI/CombatHUD`。贴图导入 `/Game/BoldersDemo/Assets/UI/CombatHUD`，由 `BP_BoldersHUD` 配置。动态填充和数字由游戏绘制。

## 职业攻击图标

屏幕底部显示本地角色所属职业的攻击图标。剑盾和普通箭以亮色及底部短金线标记。双手剑、弹射箭、爆炸箭、冰冻箭，以及法师的水、雷、火、冰都是预留项，使用较暗的图标。法师现有魔法弹是通用普攻，元素图标对应后续扩展。

`BP_BoldersHUD` 的 Loadouts 提供每个职业的图集、图标 ID、取图范围和展示状态。攻击图标用于展示，现有攻击输入、武器和技能配置继续使用原来的逻辑。重导入此次美术时运行 `Scripts/Editor/UI/Combat/setup_combat_hud.py`。

## 编辑器工具与验证

`Bolders.BuildMushroomAnimation` 创建缺失的项目动画资产；已有动画图不会自动覆盖。`Scripts/Editor/Characters/Animation/setup_character_animation_health.py` 配置角色默认类、地图 Mesh 动画模式、导入 UI 贴图并保存。蘑菇编辑器预览仍保留 PostRegisterAllComponents 的即时骨骼刷新修复。

2026-11-17：Editor Development 编译通过；双人 PIE 通过客户端真实 ServerRequestAttack RPC，第一刀后等待六秒，两端均保持 80/100 且无攻击 Montage；后续四条命令依次为 60、40、20、0。服务器和客户端均采样到受击 Montage，死亡后 bDead=true，画面中蘑菇倒地。测试世界对两个玩家分别施加伤害后，两端均为 75/100、60/100，客户端画面验证本人标识为 2P/YOU，两个血条长度与数值一致。

重新启动编辑器后，两只蘑菇均保持配置的 ABP_EnemyMushroom、动画蓝图模式及编辑器更新标志，bDead=false；已查看编辑器画面，蘑菇正常展开，没有恢复成塌缩姿势。

证据：`Saved/AnimationHealth/Verify.json`、`Snapshot.json`、`Reopen.json`、`PIE.log`。测试使用的临时位置和伤害不写回地图。尚未进行 Cook/打包、真实网络延迟或大量单位性能验证。
