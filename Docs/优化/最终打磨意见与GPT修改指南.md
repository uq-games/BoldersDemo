# BoldersDemo 最终修改与打磨意见及 GPT 执行指南

> 审查基准：当前工作区 `codex/volcano-combat` 分支源码、配置与文档（`L_VolcanoCombat` 火山铸造岛重做版本）。  
> 目的：在项目收尾阶段，基于 C++ 源码逐项排查隐性 Bug、联机不一致、操作手感短板与工程收尾项，并整理成可直接交给 GPT/Codex 执行的修改说明。

---

## 一、项目现状总评

当前版本的整体完成度已经很高：
- **玩法闭环完整**：选角预览 -> 火山铸造岛两波守卫（余烬突袭者、孢弹投手、玄武岩卫） -> 熔炉骑士两阶段 Boss 战 -> 胜负结算与重开。
- **战斗与交互有深度**：GAS 属性/抗性/元素反应表（冻结、碎冰、融化、感电、超载、蒸发）、四类环境机关（熔炉喷口、冷却槽、炸药桶、闪电导柱）联动、战士蘸刀附魔。
- **工程验证扎实**：有原生自动化测试、多轮实机回放指标和 Win64 打包验证。

但深入审计 C++ 运行时代码后，发现仍有若干**在双人联机、贴身混战、斜坡地形和 UI 交互中容易触发的隐性缺陷与打磨缺口**。把下面这些 P0/P1 项补齐后，项目即可作为一个非常干净、经得起实机试玩的最终版收官。

---

## 二、最终修改与打磨意见清单

### P0｜必改功能与联机缺陷（直接影响实战正确性）

#### 1. 友军挡弹道、贴身角色吞枪口、前排单位挡视线
- **代码位置**：
  - `Source/BoldersDemo/Private/Combat/Attacks/AttackProjectile.cpp:32, 66, 187-245`
  - `Source/BoldersDemo/Private/Combat/Abilities/BasicAttackAbility.cpp:164-168`
  - `Source/BoldersDemo/Private/AI/Positioning/AttackPositionRules.cpp:51-55`
- **问题分析**：
  1. **双人联机友军挡弹道**：`AAttackProjectile` 的 `Collision` 对 `ECC_Pawn` 设为 `ECR_Block`，而 `OnImpact` 只忽略了 `Shooter`，没有忽略同阵营队友。2P 联机时，法师/弓手从战士队友背后开火，箭矢会被队友身体直接吞掉，火球甚至会在队友背上提前引爆。
  2. **枪口穿墙检测误吞贴身射击**：`UBasicAttackAbility::OnHitEvent` 用 `ECC_Visibility` 从角色中心向枪口 `Start` 打射线，仅忽略了 `Attacker/Weapon/Target`。由于角色胶囊体阻挡 `ECC_Visibility`（`BaseCharacter.cpp:37`），当任意其他敌人或队友站在射手贴身前方时，该射线会命中该角色并直接 `return`，导致**不生成投射物、已扣冷却、无任何失败提示**。
  3. **攻击视线检测被其他角色阻挡**：`AttackPositionRules::CanAttack` 也使用 `ECC_Visibility` 检测视线，导致联机队友站前面、或小怪挡在后排/机关/落雷目标前面时，直接被判定为“攻击路线受阻”，连从天而降的**落雷（`SkyStrike`）**和穿透型**水浪（`Wave`）**都会被前排单位卡死。
- **打磨建议**：
  - 弹道碰撞忽略所有同阵营单位（或改 `ECC_Pawn` 为 Overlap 并在 Overlap 回调里仅对 `IsHostileTo` 的存活目标结算命中，同时保留对地形与机关的 Block）。
  - `CanAttack` 和枪口检测统一改用实体掩体通道（`ECC_WorldStatic | ECC_WorldDynamic | ECC_PhysicsBody`，与 `AttackResolution::HasClearPath` 对齐），不再让角色胶囊体阻挡瞄准与枪口生成。
  - 若枪口紧贴墙壁导致确实穿墙，回退到角色中心安全偏移发射（或提前在瞄准阶段拦截），不要静默吞掉冷却。

#### 2. 熔炉骑士（Boss）在坡面跳劈容易撞坡中断或悬空
- **代码位置**：`Source/BoldersDemo/Private/Characters/StatueBoss/StatueBossCharacter.cpp:431-472`
- **问题分析**：
  - `TryBeginLeap` 允许起跳点与落点存在最大 `62 cm` 的高度差（line 442），并把落点 Z 算进了 `LeapEnd`（line 455）。
  - 但 `AdvanceLeap`（line 468）强行把 `Delta.Z = 0.f`，再调用 `SafeMoveUpdatedComponent(Delta, ..., true, Blocker)`；一旦命中斜坡或台阶立面，`Blocker.IsValidBlockingHit()` 为真，Boss 会直接进入 `EStatueBossAction::Staggered`（意外打断自己），下坡跳劈则会在空中水平飘移。
- **打磨建议**：
  - `AdvanceLeap` 应使用三维插值（包含 Z 增量或带轻微起跳弧度/逐帧贴地投影），配合合理的步高/贴地处理移动；仅当水平位移确实被不可通行障碍卡住时才中止跳劈。

#### 3. 联机客户端武器缩放丢失 & 攻击前摇滑步不一致
- **代码位置**：
  - `Source/BoldersDemo/Private/Combat/CharacterCombatComponent.cpp:32-43, 108-130`
  - `Source/BoldersDemo/Private/Player/BoldersPlayerController.cpp:130-144`
  - `Source/BoldersDemo/Private/Combat/Abilities/BasicAttackAbility.cpp:84-89`
- **问题分析**：
  1. **客户端武器缩小**：三大职业的 `CombatModelScale` 均为 `1.25f`，但 `Weapon->Mesh->SetRelativeScale3D(...)` 只在服务端 `SpawnWeapon()` 中执行且不复制，同时 `Weapon` 使用 `SnapToTargetNotIncludingScale` 挂载。联机客户端看到的武器保持 1.0x，而角色模型和副手是 1.25x，比例失调。
  2. **前摇期网络延迟滑步**：直控模式下仅在客户端 `HandleCombatMovement` 检查 `OrderPhase == Windup`。在有延迟的联机环境中，客户端在收到 `Windup` 同步前发出的移动包仍会在服务端推动角色滑步。
- **打磨建议**：
  - 在 `UCharacterCombatComponent::OnRep_Equipment()` 中，基于 `Weapon` CDO 默认缩放乘以 `Character->GetCombatModelScale()` 幂等地同步主手武器缩放。
  - 在 `UBasicAttackAbility::StartMontage()` 与 `OnHitEvent()` / `EndAbility()` 之间，由服务端角色移动组件统一处理前摇定身（或抑制前摇期输入加速），确保单人/联机前摇手感完全一致。

#### 4. 全队倒下后 Boss 未被关卡管理器即时停机
- **代码位置**：`Source/BoldersDemo/Private/Game/VolcanoEncounter.cpp:314-324`
- **问题分析**：
  - 全队阵亡进入 `Defeat` 时，`AVolcanoEncounter` 停止了所有机关和守卫，但未显式停止 `Boss`。若 Boss 正在播放攻击前摇或追击，仍会残留预警/音效直到下一轮 AI 决策。
- **打磨建议**：
  - 给 `AStatueBossCharacter` 增加 `StopForEncounterEnd()`（仅权威端调用），在 `AVolcanoEncounter` 切到 `Defeat` 时立即停止 Boss 追击、清除未释放攻击预警并切至 `Idle`。

---

### P1｜核心手感、读图与交互打磨（显著提升游玩体验）

#### 5. 预输入攻击（Buffered Attack）失败时无反馈
- **代码位置**：`Source/BoldersDemo/Private/Combat/CharacterCombatComponent.cpp:195-200`
- **问题分析**：
  - 冷却剩余 < 0.44s 点击攻击会提示“已预备出招”，但定时器触发 `RunBufferedAttack()` 调用 `RequestAttack(Target)` 后直接忽略了返回值。若目标走出攻距、被地形遮挡或角色被冻结，预输入会静默失败，玩家不明白为什么没打出去。
- **打磨建议**：
  - 在 `RunBufferedAttack()` 中获取 `RequestAttack` 返回值，并通过角色所属 `ABoldersPlayerController` 回传 `ClientAttackOrderFeedback(Result)`（或在组件上暴露通知回调供 PlayerController 转发）。

#### 6. 机关可交互判定过宽，无效攻击白白消耗冷却
- **代码位置**：
  - `Source/BoldersDemo/Private/Combat/Environment/VolcanoHazard.cpp:183-193, 278-327`
  - `Source/BoldersDemo/Private/Player/BoldersPlayerControllerSelection.cpp:34-57`
  - `Source/BoldersDemo/Private/UI/Combat/BoldersHUD.cpp:137-157, 490-513`
- **问题分析**：
  - `AVolcanoHazard::CanReceiveAttack()` 未区分当前招式元素：
    - 战士（未蘸刀，物理剑）或弓手（物理箭）可以瞄准并点击**冷却槽（Puddle）**，消耗技能冷却后 `ApplyToPool(Physical)` 静默返回 `false`，无任何效果与说明；
    - 对正在 `Warning` 或 `Active` 的**熔炉喷口（Eruption）**使用物理/火/雷攻击也会空耗冷却；
    - 对已 `Frozen` 的冷却槽继续射冰箭同样空耗冷却。
- **打磨建议**：
  - 在 `AVolcanoHazard` 新增 `bool CanReactTo(EDamageElement Element) const`（按机关类型、当前状态与入射元素判断）。
  - 结合攻击者当前招式元素（含战士有效 `BladeCoating`）：
    - 在 `RequestAttack` 中，若目标机关不响应当前元素，返回新的明确反馈（如 `EAttackOrderResult::NoEffect` / 复用提示），**不进入前摇、不消耗冷却**；
    - 在 HUD 悬停提示和技能栏上方提示具体原因（例如“需水或冰压制喷口”、“需冰/火/雷激活冷却槽”）。

#### 7. 大地图/路牌弹窗与战斗操作冲突，结算面板缺少鼠标点击入口与战绩信息
- **代码位置**：
  - `Source/BoldersDemo/Private/Player/BoldersPlayerController.cpp:75, 130-144`
  - `Source/BoldersDemo/Private/Player/BoldersPlayerControllerCommands.cpp:24-35, 149-153`
  - `Source/BoldersDemo/Private/UI/Combat/BoldersHUD.cpp:747-762, 836-873`
- **问题分析**：
  1. **弹窗期间还能盲走**：展开全屏大地图（`bIslandMapExpanded`）或阅读路牌（`ReadingSign`）时，`HandleCombatMovement` 未拦截 `WASD`，玩家会在遮罩后面盲走（甚至走离路牌 350 cm 触发自动关闭）。
  2. **`R` 键在战斗中误按会空发 Reliable RPC**：`EKeys::R` 直接绑定了 `ServerRestartVolcano`，没有本地检查是否在结算阶段。
  3. **结算界面无法点击再战 & 信息偏单薄**：结算时 `HandleCommandClick` 在 line 26 提前 `return`，跳过了 `HUD->GetHitBoxAtCoordinates`，导致结算面板无法放可点击按钮；界面也只显示标题和 `R 再战`，没有通关用时和剩余生命汇总。
- **打磨建议**：
  - `ABoldersHUD` 增加 `bool IsModalOpen() const`；打开大地图或路牌时拦截 `WASD`、`SelectAttackSlot`、`RequestBladeDip` 与 `ToggleAttackRange`。
  - `EKeys::R` 改为绑定本地函数 `HandleRestartRequest()`，仅在 `!IsChoosingClass() && IsEncounterFinished(...)` 时才发 RPC。
  - 将 `HandleCommandClick` 中的 HUD HitBox 分发放在 `IsEncounterFinished` 拦截之前，并在结算面板增加可点击的 `再战` 按钮 HitBox，同时展示本局耗时与存活队员血量摘要。

#### 8. 熔炉骑士狂暴阶段特效未启用，且存在遗留未用资产/枚举
- **代码位置**：`Source/BoldersDemo/Public/Characters/StatueBoss/StatueBossCharacter.h:25-26, 129-133` 与 `StatueBossCharacter.cpp:164-173, 778-789`
- **问题分析**：
  - 构造函数加载了 `FireImpactMaterial`、`FireImpactRing`、`FireImpactParticles`，但 `PlayImpactPresentation()` 无论是否狂暴都只用了 `StoneImpactMaterial` 和 `StoneImpactParticles`。
  - 枚举中的 `EStatueBossAction::Stomp`、`FireStomp` 已无任何逻辑分支使用。
- **打磨建议**：
  - 在 `PlayImpactPresentation()` 中，当 `bEnraged == true` 时使用 `FireImpactParticles` 并叠加火焰冲击材质/火环（或在 `LeapingSlam` 狂暴时启用火焰爆发），让二阶段打击感与一阶段拉开明显层次。
  - 清理无引用的 `Stomp` / `FireStomp` 枚举值（确认蓝图无依赖后）以及任何最终未使用的材质属性。

#### 9. 双人联机队友阵亡后的体验闭环 & 小地图视觉统一
- **代码位置**：
  - `Source/BoldersDemo/Private/Game/VolcanoEncounter.cpp:261-268`
  - `Source/BoldersDemo/Private/UI/Combat/BoldersHUD.cpp:535-555, 683-690, 859-873`
- **问题分析**：
  1. 双人联机时，若一名玩家在第一波倒下，后续清波检查点 `CheckpointParty` 只回存活者血量，阵亡玩家整局无法参与，且 HUD 隐藏技能栏后没有任何等待指引。
  2. 小地图上 1P 是程序绘制的青色环+数字 `1`+白色朝向箭头，2P 队友则使用贴图圆球且缺少朝向箭头，双人同屏标识不够统一。
- **打磨建议**：
  - `CheckpointParty` 在推进波次（波次 1 -> 2，波次 2 -> Boss）时，将以 50% 血量复活倒地队友（恢复血量、启用移动模式 `MOVE_Walking`，并重置冻结/异常光环）；倒地期间在 HUD 底部显示“已倒下，推进至下一阶段后复活”提示。
  - 统一小地图 1P/2P 的圆标与朝向箭头绘制逻辑（两位玩家均带朝向指示或按统一风格区分）。

---

### P2 / P3｜工程健壮性、配置清理、测试与文档收尾

#### 10. 性能与代码健壮性小修
- **每帧全场景遍历缓存**：`BoldersHUD::DrawHUD`、`BoldersPlayerController::HandleCombatMovement`、`HandleCommandClick` 高频调用 `AVolcanoEncounter::Find(GetWorld())`（内部是 `TActorIterator`）。建议在 `ABoldersPlayerController` 和 `ABoldersHUD` 中缓存 `TWeakObjectPtr<AVolcanoEncounter>`。
- **资产数组序列化防护**：`AVolcanoHazard` 的 `FixtureMeshes` / `FixtureEffects` 是由构造函数填充的非 Transient `UPROPERTY()`，且代码中多处直接下标读取（如 `FixtureMeshes[Vent]`、`FixtureEffects[ArcLink]`）。应标记为 `Transient` 并在读取时使用 `IsValidIndex` 保护，防止关卡实例序列化旧数组导致越界。
- **服务器时钟统一**：`AVolcanoEncounter::StartWave()` line 231 使用 `GetWorld()->GetTimeSeconds()`，与 `Tick()` line 274 的 `GameState->GetServerWorldTimeSeconds()` 不一致，建议将 `Now` 作为参数传入 `StartWave(int32 WaveNumber, double Now)`。

#### 11. 配置文件与打包稳定性收尾
- **`Config/DefaultEngine.ini` 重复项清理**：
  - Lines 40–41 重复配置了 `DefaultGraphicsRHI=DefaultGraphicsRHI_DX12`；
  - Lines 93–94 重复配置了 `+ActiveGameNameRedirects=(OldGameName="TP_Blank",NewGameName="/Script/BoldersDemo")`。
- **渲染特性与打包退出稳定性复核**：
  - `Config/DefaultEngine.ini` 当前默认开启了 `r.RayTracing=True`、`r.RayTracing.RayTracingProxies.ProjectEnabled=True` 和 `r.Substrate=True`，同时 `VolcanoCombatPlaytest.md` 记录到独立包离屏退出需依赖外部启动脚本关闭 `r.PSOPrecaching=0`。若关卡本身不依赖硬件光追，建议评估关闭未使用的光追开关并复测独立包默认启动/退出是否稳定退出码 0，同时降低显存和编辑器帧耗时 P95（当前 25–29 ms）。

#### 12. 补齐重做后的核心回归测试 & 同步文档与 Git 状态
- **新增原生自动化测试**：目前 5 项测试集中在旧版机制，建议在 `Source/BoldersDemo/Private/Tests/Combat/` 补写 1–2 个测试覆盖：
  1. 联机友军不挡弹道 & 贴身角色不吞枪口 & 视线不被 Pawn 阻挡；
  2. 冷却槽元素响应过滤（物理攻击不空耗冷却）+ 战士蘸刀附魔与单次消耗；
  3. Boss 斜坡跳劈移动不因高低差误入 `Staggered`。
- **同步 `Docs/CodeGuide.md`**：补充火山铸造岛重做后的主流程（WASD 直控、`VolcanoEncounter`、`StatueBossCharacter`、`VolcanoHazard`、`VolcanoWaySign`、`BladeCoating`），不再只停留在旧版 `TestMap` 点击移动。
- **Git 清理**：当前 `codex/volcano-combat` 分支有大量未提交修改与未跟踪产物（含 `Build/Windows/FileOpenOrder/*.log`），收尾验收通过后应确认 `.gitignore` 规则并按模块整理提交。

---

## 三、给 GPT（Codex）的直接修改指令

> 下面这段可以直接复制发给 GPT/Codex，按优先级依次执行。

```markdown
请对当前 Unreal Engine 5 项目 `BoldersDemo`（分支 `codex/volcano-combat`）执行最终打磨修改。

### 全局约束
1. 严格遵守仓库根目录 `AGENTS.md` 中的资产与源码放置规则。
2. 保持所有游戏规则权威在服务端（`HasAuthority()`），不得破坏双人 Listen Server 同步。
3. 不得引入未受 `#if WITH_EDITOR` 保护的编辑器依赖，确保 `BoldersDemoEditor` 和 `BoldersDemo`（Win64 Development）都能顺利编译。
4. 保留现有 5 项 `BoldersDemo.*` 原生自动化测试全部通过，并按任务补齐新增回归测试。

---

### 任务 1（P0）：修复友军挡弹道、贴身吞枪口与角色挡视线
- **文件**：
  - `Source/BoldersDemo/Private/Combat/Attacks/AttackProjectile.cpp`
  - `Source/BoldersDemo/Private/Combat/Abilities/BasicAttackAbility.cpp`
  - `Source/BoldersDemo/Private/AI/Positioning/AttackPositionRules.cpp`
- **怎么改**：
  1. `AAttackProjectile::InitializeShot` / `InitializeModuleShot`：
     - 改为对非敌对角色不产生阻挡碰撞。建议方案：遍历世界中同阵营 `ABaseCharacter` 调用 `Collision->IgnoreActorWhenMoving(Ally, true)`，**并且**把普通弹道（非 `Wave`）对 `ECC_Pawn` 的响应改为 `ECR_Overlap`（绑定 `OnComponentBeginOverlap` 仅处理 `IsValid(Victim) && Shooter->IsHostileTo(Victim) && Victim->IsAlive()` 的目标并调用统一命中逻辑），保留 `WorldStatic / WorldDynamic / PhysicsBody / Interactable` 为 `ECR_Block` 走 `OnImpact`。这样即便队友在飞行途中移动或生成，也不会吞掉箭矢或提前引爆火球。
  2. `UBasicAttackAbility::OnHitEvent`（lines 164–168）：
     - 将枪口防穿墙射线从 `LineTraceTestByChannel(..., ECC_Visibility, Params)` 改为 `LineTraceTestByObjectType(..., SolidObjects, Params)`（仅包含 `ECC_WorldStatic | ECC_WorldDynamic | ECC_PhysicsBody`，不要包含 `ECC_Pawn`）。
     - 若检测到枪口确实穿过静态墙体，则将 `Start` 回退至 `Attacker->GetActorLocation()` 与墙面命中点之间安全缩进处（或向后退 15 cm），避免已扣冷却却静默不发射。
  3. `AttackPositionRules::CanAttack`（lines 51–55）：
     - 将视线检测改为仅检测实体掩体（`ECC_WorldStatic | ECC_WorldDynamic | ECC_PhysicsBody`，与 `AttackResolution::HasClearPath` 一致），不再使用会被角色胶囊体阻挡的 `ECC_Visibility`。注意保留忽略 `Attacker` 和 `Target`。

### 任务 2（P0）：修复 Boss 斜坡跳劈撞坡误打断
- **文件**：`Source/BoldersDemo/Private/Characters/StatueBoss/StatueBossCharacter.cpp`
- **怎么改**：
  1. 修改 `AStatueBossCharacter::AdvanceLeap(double Elapsed)`（lines 460–472）：
     - 不要强制将 `Delta.Z = 0.f`。改为先计算当前步的水平目标位置 `DestinationXY`，并按 `LeapStart.Z -> LeapEnd.Z` 插值目标高度（或叠加少量起跳抬升/对地面做向下 `ECC_WorldStatic` 采样得到当前脚底高度 + 胶囊半高），得到完整三维 `Delta`。
     - 上坡移动时先向上微抬或使用 `SlideAlongSurface` 沿可走坡面滑行，避免因斜坡法线触发 `Blocker.IsValidBlockingHit()` 误把跳劈打断为 `Staggered`。仅当水平位移真正被不可通行墙体阻挡且离目标点超过阈值时才返回 `false`。

### 任务 3（P0）：修复联机武器缩放不同步、前摇期移动一致性与团灭停 Boss
- **文件**：
  - `Source/BoldersDemo/Private/Combat/CharacterCombatComponent.cpp`
  - `Source/BoldersDemo/Private/Combat/Abilities/BasicAttackAbility.cpp`
  - `Source/BoldersDemo/Public/Characters/StatueBoss/StatueBossCharacter.h`
  - `Source/BoldersDemo/Private/Characters/StatueBoss/StatueBossCharacter.cpp`
  - `Source/BoldersDemo/Private/Game/VolcanoEncounter.cpp`
- **怎么改**：
  1. 在 `UCharacterCombatComponent::OnRep_Equipment()` 中，对 `Weapon->Mesh` 幂等地应用角色缩放：读取 `Weapon->GetClass()->GetDefaultObject<AWeapon>()->Mesh->GetRelativeScale3D() * Character->GetCombatModelScale()`，移除 `SpawnWeapon()` 中只在服务端累乘缩放的逻辑，保证主机与客户端武器比例一致。
  2. 在 `UBasicAttackAbility` 中，当 `Attacker->HasDirectCombatInput()` 时，在 `StartMontage()` 前摇阶段通过服务端一致约束移动（例如暂停行走输入或临时锁住水平加速度，在 `OnHitEvent()` 命中帧和 `EndAbility()` 中保证恢复），消除联机延迟下的前摇滑步。
  3. 为 `AStatueBossCharacter` 新增 `void StopForEncounterEnd()`（仅 `HasAuthority()` 生效）：若 Boss 存活且非 `Defeated`，立即 `StopPursuit()`，清空 `bEnrageQueued`，并将状态切回 `Idle`（关闭任何进行中的攻击预警）。在 `AVolcanoEncounter::UpdateEncounter` 进入 `Defeat` 时调用 `if (IsValid(Boss)) Boss->StopForEncounterEnd();`。

### 任务 4（P1）：补全预输入失败反馈 & 机关无效元素防误触
- **文件**：
  - `Source/BoldersDemo/Public/Combat/Data/AttackOrder.h`
  - `Source/BoldersDemo/Public/Combat/Environment/VolcanoHazard.h`
  - `Source/BoldersDemo/Private/Combat/Environment/VolcanoHazard.cpp`
  - `Source/BoldersDemo/Private/Combat/CharacterCombatComponent.cpp`
  - `Source/BoldersDemo/Private/AI/Positioning/AttackPositionRules.cpp`
  - `Source/BoldersDemo/Private/UI/Combat/BoldersHUD.cpp`
- **怎么改**：
  1. 在 `UCharacterCombatComponent::RunBufferedAttack()` 中捕获 `const EAttackOrderResult Result = RequestAttack(Target);`，若角色由 `ABoldersPlayerController` 控制，调用其 `ClientAttackOrderFeedback(Result)`（或在 Controller 中提供内部转发方法），确保预输入失效时有 HUD 提示。
  2. 在 `AVolcanoHazard` 新增 `bool CanReactTo(EDamageElement Element) const`：
     - `Eruption`：`Water/Ice` 始终可响应；其他元素仅在 `Cooling` 或 `Ready` 时可提前触发。
     - `Puddle`：`Lightning` 仅在非 `Steaming/Frozen/Active` 时有效；`Ice` 仅在非 `Steaming/Frozen` 时有效；`Fire` 仅在非 `Steaming` 时有效；`Physical/Water` 无效。
     - `ExplosiveBarrel` / `LightningOrb`：`State == Ready` 时任意元素有效。
  3. 在 `UCharacterCombatComponent::RequestAttack` 中计算本次实际攻击元素（若当前模块为物理且战士有有效 `BladeCoating`，取 `BladeCoating`），若目标是 `AVolcanoHazard` 且 `!Hazard->CanReactTo(EffectiveElement)`，返回新增枚举 `EAttackOrderResult::Ineffective`（不消耗冷却、不放前摇），并在 `BoldersHUD` 的 `GetOrderLabel` 与悬停提示中显示明确中文说明（例如“当前招式对该机关无效”）。

### 任务 5（P1）：修复弹窗输入冲突、优化结算交互与 `R` 键守卫
- **文件**：
  - `Source/BoldersDemo/Public/UI/Combat/BoldersHUD.h`
  - `Source/BoldersDemo/Private/UI/Combat/BoldersHUD.cpp`
  - `Source/BoldersDemo/Private/Player/BoldersPlayerController.cpp`
  - `Source/BoldersDemo/Private/Player/BoldersPlayerControllerCommands.cpp`
- **怎么改**：
  1. `ABoldersHUD` 增加 `bool IsModalOpen() const { return bIslandMapExpanded || ReadingSign.IsValid(); }`。
  2. `ABoldersPlayerController::HandleCombatMovement`、`SelectAttackSlot`、`RequestBladeDip`、`ToggleAttackRange` 在 `HUD && HUD->IsModalOpen()` 时直接返回，避免打开大地图或读路牌时角色盲走或误切技能。
  3. `ABoldersPlayerController::SetupInputComponent` 中 `EKeys::R` 改为绑定本地方法 `HandleRestartRequest()`，仅在 `!IsChoosingClass() && IsEncounterFinished(GetEncounter())` 时调用 `ServerRestartVolcano()`。
  4. `ABoldersPlayerController::HandleCommandClick`：将 HUD HitBox 命中检测挪到 `IsEncounterFinished` 判断之前；在 `ABoldersHUD::DrawVolcano` 的 `Victory/Defeat` 结算面板中：
     - 增加可点击的 `RestartMatch` HitBox（点击触发 `PC->HandleRestartRequest()`），保留 `R` 快捷键；
     - 显示本局耗时（由 `AVolcanoEncounter` 记录开始与结束时间戳并同步给客户端）及队员存活/血量摘要。

### 任务 6（P1）：补齐 Boss 狂暴火焰冲击表现与双人检查点复活
- **文件**：
  - `Source/BoldersDemo/Public/Characters/StatueBoss/StatueBossCharacter.h`
  - `Source/BoldersDemo/Private/Characters/StatueBoss/StatueBossCharacter.cpp`
  - `Source/BoldersDemo/Private/Game/VolcanoEncounter.cpp`
  - `Source/BoldersDemo/Private/UI/Combat/BoldersHUD.cpp`
- **怎么改**：
  1. 在 `AStatueBossCharacter::PlayImpactPresentation()` 中，当 `bEnraged` 为真时启用已加载的 `FireImpactMaterial`、`FireImpactParticles` 和 `FireImpactRing`（与一阶段岩石特效区分开）；删除 `EStatueBossAction` 中废弃未用的 `Stomp` 和 `FireStomp`（请先全库搜索确认无蓝图或测试引用）。
  2. 在 `AVolcanoEncounter::CheckpointParty` 中，遍历全队玩家（包含已倒地的 `PlayerSlot > 0` 角色）：
     - 存活成员照常恢复 30% 最大生命；
     - 倒地成员在推进波次时就地（或移至最近存活队友旁安全导航点）以 50% 最大生命复活，重新启用 `SetMovementMode(MOVE_Walking)` 并清理异常光环。
     - 角色倒地且关卡尚未失败时，在 `ABoldersHUD` 底部显示简洁状态提示（“已倒下，推进至下一阶段后复活”）。
  3. 统一 `ABoldersHUD::DrawIslandMinimap` 中 1P 与 2P 的小地图标记和朝向箭头绘制逻辑。

### 任务 7（P2/P3）：工程清理、缓存优化、补齐测试与同步文档
- **文件**：
  - `Source/BoldersDemo/Public/Combat/Environment/VolcanoHazard.h`
  - `Source/BoldersDemo/Private/Combat/Environment/VolcanoHazardPresentation.cpp`
  - `Source/BoldersDemo/Private/Game/VolcanoEncounter.cpp`
  - `Source/BoldersDemo/Private/Player/BoldersPlayerController.cpp`
  - `Source/BoldersDemo/Private/UI/Combat/BoldersHUD.cpp`
  - `Config/DefaultEngine.ini`
  - `Source/BoldersDemo/Private/Tests/Combat/`
  - `Docs/CodeGuide.md`
- **怎么改**：
  1. `AVolcanoHazard` 的 `FixtureMeshes`、`FixtureEffects` 标记为 `UPROPERTY(Transient)`，所有下标访问增加 `IsValidIndex` 防御。
  2. `AVolcanoEncounter::StartWave(int32 WaveNumber, double Now)` 统一使用传入的服务器时间 `Now`，不再混用 `GetWorld()->GetTimeSeconds()`。
  3. 在 `ABoldersPlayerController` 和 `ABoldersHUD` 中缓存 `TWeakObjectPtr<AVolcanoEncounter>`，消除每帧 `TActorIterator`。
  4. 清理 `Config/DefaultEngine.ini` 中重复的 `DefaultGraphicsRHI=DefaultGraphicsRHI_DX12`（40–41 行）和重复的 `ActiveGameNameRedirects`（93–94 行）；检查并评估关闭未使用的 `r.RayTracing` 开关后独立包启动与退出码是否为 0。
  5. 新增 1–2 个原生自动化测试，覆盖：
     - 友军不阻挡弹道、贴身单位不吞枪口、视线不被 Pawn 阻挡；
     - 冷却槽元素响应校验（物理攻击返回无效且不消耗冷却）与战士蘸刀消耗；
     - Boss 斜坡跳劈不会因高度差进入 `Staggered`。
  6. 更新 `Docs/CodeGuide.md`，补充火山铸造岛的 WASD 直控、波次推进、Boss 状态机、机关交互与路牌阅读入口。

---

### 验收标准
1. `BoldersDemoEditor Win64 Development` 与 `BoldersDemo Win64 Development` 编译 0 错误、无新增告警。
2. Session Frontend / 命令行运行 `BoldersDemo.` 全部原生自动化测试 100% 通过（原有 5 项 + 新增回归测试）。
3. 实机 PIE 验证：
   - 单人模式下：打开大地图/路牌时 `WASD` 不盲走；战士物理剑点冷却槽不浪费冷却且有提示；Boss 在坡道追击跳劈正常落地；Boss 狂暴后砸地有火焰特效；通关结算可鼠标点击“再战”或按 `R` 重开。
   - 双人 Listen Server 模式下：1P/2P 武器尺寸比例正常；远程玩家穿过战士队友射击可正常命中后排敌人与机关；一名玩家首波阵亡后在清波推进时可正常复活。
4. 记录实机验证结果到 `Docs/Maps/VolcanoCombatPlaytest.md` 的最终收尾小节，并整理干净待提交的 git 文件清单。
```
