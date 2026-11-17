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

沿用已有 ABoldersHUD Canvas 绘制架构；没有额外引入第二套 HUD。两位玩家按 PlayerSlot 排序显示在左上角，通过本地 PlayerController 的 AssignedCharacter 标记 YOU/ALLY。敌方血条按胶囊顶部位置投影到屏幕，离开屏幕或在相机后方时不绘制。死亡显示空血条。

生命值与最大生命值直接读取角色 GAS AttributeSet；使用既有属性复制，不再维护独立可写的 UI 生命值。队伍血条颜色与 1P/2P 的青色、金色标记一致，敌方为红色。当前是 demo 的屏幕空间 HUD，未加入墙体遮挡隐藏或大量单位的布局防重叠。

GPT Image 生成的边框导入为 `/Game/BoldersDemo/Assets/UI/Health/T_HealthBarFrame`，在 BP_BoldersHUD 的 HealthBarFrame 上配置。原图与完整提示词见 `ArtSources/UI/README.md`。动态填充和数字由游戏绘制。

## 编辑器工具与验证

`Bolders.BuildMushroomAnimation` 创建缺失的项目动画资产；已有动画图不会自动覆盖。`Scripts/Editor/setup_character_animation_health.py` 配置角色默认类、地图 Mesh 动画模式、导入 UI 贴图并保存。蘑菇编辑器预览仍保留 PostRegisterAllComponents 的即时骨骼刷新修复。

2026-11-17：Editor Development 编译通过；双人 PIE 通过客户端真实 ServerRequestAttack RPC，第一刀后等待六秒，两端均保持 80/100 且无攻击 Montage；后续四条命令依次为 60、40、20、0。服务器和客户端均采样到受击 Montage，死亡后 bDead=true，画面中蘑菇倒地。测试世界对两个玩家分别施加伤害后，两端均为 75/100、60/100，客户端画面验证本人标识为 2P/YOU，两个血条长度与数值一致。

重新启动编辑器后，两只蘑菇均保持配置的 ABP_EnemyMushroom、动画蓝图模式及编辑器更新标志，bDead=false；已查看编辑器画面，蘑菇正常展开，没有恢复成塌缩姿势。

证据：`Saved/AnimationHealth/Verify.json`、`Snapshot.json`、`Reopen.json`、`PIE.log`。测试使用的临时位置和伤害不写回地图。尚未进行 Cook/打包、真实网络延迟或大量单位性能验证。
