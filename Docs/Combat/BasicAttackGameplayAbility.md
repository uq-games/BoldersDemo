# 普通攻击 Gameplay Ability

## 当前控制与网络边界

战士由服务器上的 AIController 控制，玩家 PlayerController 控制 CameraPawn，通过 AssignedCharacter 下达命令。客户端点击敌人后，PlayerController 的 ServerRequestAttack 先检查 Attacker 是否为自己的 AssignedCharacter；服务器再向该战士 ASC 发送 Event.Ability.BasicAttack。普攻能力是 ServerOnly，不做客户端预测；不要把战士假设成该玩家网络拥有的 Pawn。

BaseCharacter 的 ASC OwnerActor 和 AvatarActor 当前均为角色自身，在 BeginPlay 中初始化。Combat 组件在组件 BeginPlay 创建武器；角色完成 ASC 初始化后，仅服务器为有武器且配置了 Ability 数据的角色授予一次能力，并保存 SpecHandle。`BP_Warrior` 的 Combat 组件在 `Combat|Abilities` 中引用 `/Game/BoldersDemo/Assets/Combat/DA_WarriorAbilities`，其普攻字段配置 `/Game/BoldersDemo/Blueprints/Abilities/GA_BasicAttack` 和等级 1。该蓝图继承 C++ 的 `UBasicAttackAbility`，目前能力执行逻辑仍由父类实现；数据资产保存配置，创建它不自动授予，运行时能力和句柄由 ASC 管理。EndPlay 清除授予的能力和武器。未配置武器的蘑菇不会获得普攻能力。

数据资产中的普攻配置类型为 `TSubclassOf<UBasicAttackAbility>`，限制为兼容当前事件和执行逻辑的子类。未配置数据资产、普攻类或有效等级时不会静默授予 C++ 类，而会在服务器输出警告；若将蓝图触发设置或执行逻辑改写，需重新验证与现有事件及能力生命周期的契约。它目前只管理普攻类与等级，伤害和 Montage 仍由武器配置，运行中的生命值仍由 ASC/AttributeSet 持有。

Data Asset 版本：Editor Development 编译通过；两人 listen-server PIE 中服务器日志确认两位战士从 `DA_WarriorAbilities` 授予 `GA_BasicAttack_C`、等级 1。近距离攻击的 Montage 两端可见，一次命令生命值两端均从 100 降至 80；远距离接近途中取消后目标仍为 100，再次攻击可接近并命中一次。重开编辑器后检查 BP_Warrior → DA_WarriorAbilities → GA_BasicAttack 的引用链和等级均保留。资产通过编辑器接口迁移，资产引用查询显示 Data Asset 引用新 Ability 路径，BP_Warrior 引用 Data Asset。证据 `Saved/AbilityData/Montage.json`、`Approach.json`、`PIE.log`。尚未覆盖 Cook/打包、真实网络延迟。

## 一次普攻

1. Combat 验证角色存活、目标有效且敌对、武器与 Montage 已配置，然后取消此前的普攻能力，发送带目标的 GameplayEvent。
2. BasicAttackAbility 从 EventData 读取目标，再次在服务器验证，CommitAbility。进入攻击范围则直接攻击，否则将 EQS 查询和移动交给 AttackPositioningComponent。
3. 到位回调重新检查距离、遮挡和双方状态，选一次 Montage。WaitGameplayEvent 监听 Event.Montage.AttackHit；PlayMontageAndWait 播放 Montage，ASC 复制动画给客户端。
4. AnimNotify 只在服务器向自身 ASC 发送命中事件。Ability 只处理一次事件，重新检查后调用角色原有伤害入口，应用 GameplayEffect 改变 GAS Health。
5. Montage 正常结束、被打断、目标不可达或外部取消时结束 Ability。EndAbility 清除本次状态和自身的 EQS 移动；AbilityTask 自动清理事件监听和 Montage。

地面移动和死亡继续调用 Combat.CancelAttack：它通过 SpecHandle 取消 Ability。移动组件的取消逻辑只中止自己的路径请求，防止取消新下达的普通移动。敌人受击、死亡、血条仍消费原有 Health，不在 Ability 中复制第二套生命值。

## 学习重点

- Gameplay Ability 管一次行动的开始、异步等待、结束和取消；Ability Task 管等待 Montage 和 Gameplay Event。
- Gameplay Event 是带 Tag 的消息，Payload 在本例中携带目标。命中事件不相信客户端，只有服务器发送并应用伤害。
- ASC 的角色初始化、授予 Ability、激活 Ability 是三个不同步骤。授予后不等于立即攻击。
- 现在没有费用或冷却，但调用 CommitAbility 保留后续扩展入口；攻击距离和伤害仍由武器提供。
- 不要把蓝图中的 EQS 配置改到 Ability 上：当前仍读取 Combat.AttackPositionQuery，避免打断现有配置。

本次不实现预测、网络延迟补偿、剑刃碰撞或多目标技能。伤害仍按距离和遮挡检查；连续点击会取消旧命令并开启新命令。

## 定向验证（2026-11-17）

- Editor Development 编译链接通过。双人 listen-server PIE 中从客户端调用 PlayerController 的 ServerRequestAttack：一条近距离攻击命令在六秒后仍只造成一次伤害，两端 80/100；分别采样到服务器与客户端的攻击 Montage 和客户端蘑菇受击 Montage。证据 `Saved/AnimationHealth/GASMontage.json`。
- 五次独立近距离命令两端依次为 80、60、40、20、0，死亡状态一致。证据 `Saved/AnimationHealth/GASAttack.json`。
- 客户端从远处发起攻击并在移动途中发出合法地面移动命令：取消旧接近，战士移动到新目标，敌人仍为 100。随后再次攻击，EQS 成功选择站位、战士接近并命中一次；四秒后仍为 80。证据 `Saved/AnimationHealth/GASApproach.json`。
- 将目标临时放出导航投影范围，查询返回无站位，角色与目标生命保持不变。上述临时位置只在 PIE 生效。首次取消测试的地面位置被碰撞挡住而拒绝，不能算通过；已改用合法位置重测。

这些是定向 PIE 测试，尚未验证真实网络延迟、Cook/打包，也未覆盖 Montage 已开始后取消的全部时序和高密度动态单位。
