# P2 核心代码与半 AI 开发实践：Rider 截图清单、口播稿与 ChatGPT 必剪全自动指令

> **目标受众**：UE5 玩法 / 战斗系统客户端面试官  
> **核心叙事**：坦诚说明项目采用 **半 AI 辅助开发**——明确展示哪些重复性模板交给 AI 提速，重点用真实的 **GAS 架构、Listen Server 网络同步 Review 修坑、性能优化与原生 Automation Test** 证明自己的底层理解与工程把关能力。  
> **画面规格**：纯 JetBrains Rider 高清代码截图（13 张，17pt 字体，每张 14~45 行 + 简洁红框标注），加极简片头/片尾卡，总长约 `03:45`（`1920×1080`，`30/60 fps`，MP4）。  
> **口播风格**：说人话、不堆名词、直接用 `GAS / AbilitySystem / GameplayTag / Listen Server / RPC / Overlap / NavMesh / FAnimInstanceProxy / Automation Test` 等原生术语；必剪 AI 配音 **「曼波讲故事」**，语速 `1.0x`，句间留足 `2.5 ~ 3 秒` 自然停顿，左上角每次只显示 **1 个无加点单标签**。

---

## 一、13 张 Rider 精准截图清单（输出至 `Saved\DemoVideo\02_RiderScreenshots\`）

| 序号 | 截图文件名 | 精确文件路径 | 屏幕显示行号 | 必剪高亮红框行号 |
| --- | --- | --- | :---: | --- |
| 1 | `P2_01_AgentsRules.png` | `AGENTS.md`（**唯一保留左侧目录树的一张**，展开 `Source/BoldersDemo/Private` 一级子目录） | `L1–L38` | 左侧 `Characters / Combat / Game / Player / Tests` 目录区 |
| 2 | `P2_02_AITemplateCode.png` | `Source/BoldersDemo/Private/Editor/Combat/FoundryParticleSetup.cpp` | `L171–L215` | `L175–L207` 批量粒子参数表；`L212–L214` 控制台命令 |
| 3 | `P2_03_AttackLoadout.png` | `Source/BoldersDemo/Public/Combat/Data/AttackLoadout.h` | `L14–L49` | `L15–L24` 元素与攻击形态；`L33–L49` 数据资产字段 |
| 4 | `P2_04_DamageExecution.png` | `Source/BoldersDemo/Private/Combat/Effects/BoldersDamageEffect.cpp` | `L46–L81` | `L49–L53` 抗性捕获；`L63–L74` 按动态 Tag 结算实伤 |
| 5 | `P2_05_ReactionTable.png` | `Source/BoldersDemo/Private/Combat/Attacks/ElementalHit.cpp` | `L20–L48` | `L32–L47` 静态规则表 9 种反应（正反顺序通用） |
| 6 | `P2_06_ReactionApply.png` | `Source/BoldersDemo/Private/Combat/Attacks/ElementalHit.cpp` | `L75–L111` | `L100–L108` 清旧附着与 `FElementalHitBatch` 防重入 |
| 7 | `P2_07_ProjectileOverlap.png` | `Source/BoldersDemo/Private/Combat/Attacks/AttackProjectile.cpp` | `L163–L204` | `L193–L203` `OnPawnOverlap` 仅命中敌对存活目标 |
| 8 | `P2_08_SafeMuzzle.png` | `Source/BoldersDemo/Private/Combat/Abilities/BasicAttackAbility.cpp` | `L202–L229` | `L219–L228` `SolidObjects` 球体扫掠与贴墙发射点回缩 |
| 9 | `P2_09_WindupMovement.png` | `Source/BoldersDemo/Private/Characters/BoldersCombatMovementComponent.cpp` | `L1–L14` | `L6–L14` `ConstrainInputAcceleration` 在 `Windup` 归零 |
| 10 | `P2_10_FactionRingOpt.png` | `Source/BoldersDemo/Private/Characters/Presentation/FactionRingComponent.cpp` | `L97–L120` | `L104–L113` 固定地面静止节流 + 保留 Tick 与动平台检测 |
| 11 | `P2_11_BossLeapSlope.png` | `Source/BoldersDemo/Private/Characters/StatueBoss/StatueBossCharacter.cpp` | `L467–L496` | `L477–L493` 逐帧地面高度采样 + 坡面法线滑行 |
| 12 | `P2_12_BossAnimProxy.png` | `Source/BoldersDemo/Private/Characters/StatueBoss/StatueBossAnimInstance.cpp` | `L10–L52` | `L14–L45` `PreUpdate` 拷贝与 `Evaluate` 工作线程混合 |
| 13 | `P2_13_AutomationTests.png` | `Source/BoldersDemo/Private/Tests/Combat/VolcanoReleaseTest.cpp` | `L39–L79` | `L54–L78` 自动化校验友军穿透、不挡视线与实墙阻挡 |

---

## 二、P2 完整时间轴、左上角单标签与「曼波讲故事」口播表（共 24 句，约 `03:45`）

> **画面与时间轴规则**：
> - `00:00–00:06`：片头深色静态卡（居中白字：`BoldersDemo P2` / `代码架构与半 AI 开发实践`）
> - `00:06–03:37`：依次平切 13 张 Rider 截图（每张停留时间严格覆盖对应的 1~2 句口播及句后留白）
> - `03:37–03:45`：片尾深色静态卡（居中白字：`感谢观看` / `BoldersDemo UE5 C++`）
> - 左上角标签：**每次只显示 1 个词组，严禁使用 `·`、`•`、`|` 拼接多个词**。

| 序号 | 画面素材 | 开始时间 | 预计说完 | 句后留白 | 左上角单标签 | 「曼波讲故事」口播文案（直接复制） |
| --- | --- | :---: | :---: | :---: | --- | --- |
| **01** | 片头卡（`00:00–00:06`） | `00:00` | `00:04` | 2 秒 | （无标签） | 大家好，这期是 P2，主要讲这个项目的核心代码，以及我在半 AI 开发里怎么把关。 |
| **02** | `P2_01_AgentsRules.png`（`00:06–00:23`） | `00:07` | `00:13` | 3 秒 | `工程目录规范` | 先说开发方式。这个项目是半 AI 辅助完成的，但我先定好了目录和资产规范，防止 AI 把代码写乱。 |
| **03** | `P2_01_AgentsRules.png` | `00:16` | `00:21` | 2.5 秒 | `工程目录规范` | 像左边这样按角色、战斗、机关、网络和测试拆开，每个模块的职责边界都很清楚。 |
| **04** | `P2_02_AITemplateCode.png`（`00:23–00:41`） | `00:24` | `00:30` | 3 秒 | `AI 模板代码` | 像这种批量生成粒子资产、导入动画和基础样板的重复代码，我直接交给 AI 提速。 |
| **05** | `P2_02_AITemplateCode.png` | `00:33` | `00:38` | 3 秒 | `AI 模板代码` | 我自己把精力放在核心架构设计、代码 Review 修坑和联机验收上。 |
| **06** | `P2_03_AttackLoadout.png`（`00:41–00:50`） | `00:42` | `00:48` | 2.5 秒 | `GAS 数据驱动` | 核心战斗基于 GAS 实现。三职业的十套招式都收口在这个数据资产里，新增技能不用复制代码。 |
| **07** | `P2_04_DamageExecution.png`（`00:50–01:08`） | `00:51` | `00:56` | 3 秒 | `自定义伤害计算` | 伤害结算写了自定义 Execution，启动时捕获五种抗性属性。 |
| **08** | `P2_04_DamageExecution.png` | `00:59` | `01:06` | 2.5 秒 | `自定义伤害计算` | 每次命中按 GameplayTag 找到对应抗性，像 Boss 弱雷抗和重甲降甲，改属性就能生效。 |
| **09** | `P2_05_ReactionTable.png`（`01:08–01:17`） | `01:09` | `01:15` | 2.5 秒 | `元素反应表` | 九种元素反应没有写一堆 if 分支，而是做成一张静态规则表，正反顺序都能命中。 |
| **10** | `P2_06_ReactionApply.png`（`01:17–01:28`） | `01:18` | `01:25` | 3 秒 | `防重复连锁` | 结算时会先按倍率扣血再清旧附着，并用批次上下文记录目标，防止范围技能反复触发感电连锁。 |
| **11** | `P2_07_ProjectileOverlap.png`（`01:28–01:46`） | `01:29` | `01:35` | 3 秒 | `修坑一 友军挡弹` | 接下来讲网络同步里我 Review 出来的几个真坑。第一版投射物对角色设了阻挡，双人联机时队友会把箭直接吞掉。 |
| **12** | `P2_07_ProjectileOverlap.png` | `01:38` | `01:44` | 2.5 秒 | `修坑一 友军挡弹` | 排查后我把角色通道改成 Overlap，只在重叠回调里过滤敌对存活目标，弓手隔着队友就能正常穿射。 |
| **13** | `P2_08_SafeMuzzle.png`（`01:46–02:05`） | `01:47` | `01:54` | 3 秒 | `修坑二 枪口吞弹` | 第二个坑是枪口防穿墙原来用的 Visibility 通道，有怪贴脸时会把射击吞掉还照样扣冷却。 |
| **14** | `P2_08_SafeMuzzle.png` | `01:57` | `02:03` | 2.5 秒 | `修坑二 枪口吞弹` | 改用实体掩体通道做球体扫掠后，贴身角色不再挡枪口，紧贴墙面时发射点也会自动回缩。 |
| **15** | `P2_09_WindupMovement.png`（`02:05–02:25`） | `02:06` | `02:13` | 3 秒 | `修坑三 前摇滑步` | 第三个坑是攻击前摇本来只在客户端禁用了按键，联机一有延迟，服务端依然会带着角色滑步。 |
| **16** | `P2_09_WindupMovement.png` | `02:16` | `02:23` | 2.5 秒 | `修坑三 前摇滑步` | 我直接继承移动组件重写了输入加速度约束，服务端重放移动时同样生效，百毫秒延迟下前摇滑步降到零。 |
| **17** | `P2_10_FactionRingOpt.png`（`02:25–02:46`） | `02:26` | `02:33` | 3 秒 | `地面射线节流` | 性能优化方面，角色脚底圆环原来每帧都打射线找地面，站着不动每秒也要查六十次。 |
| **18** | `P2_10_FactionRingOpt.png` | `02:36` | `02:44` | 2.5 秒 | `地面射线节流` | 我加了静止固定地面的缓存节流，每秒降到五次，同时保留可移动平台的每帧检测，防止站在动平台上圆环脱节。 |
| **19** | `P2_11_BossLeapSlope.png`（`02:46–03:06`） | `02:47` | `02:54` | 3 秒 | `斜坡跳劈修复` | 雕像 Boss 跳劈早期版本把 Z 轴位移写死成零，一上斜坡就会撞到坡面提前打断。 |
| **20** | `P2_11_BossLeapSlope.png` | `02:57` | `03:04` | 2.5 秒 | `斜坡跳劈修复` | 修成逐帧采样地面高度加沿坡面法线滑行后，上下坡都能稳稳落地，只有撞到实墙才会失衡。 |
| **21** | `P2_12_BossAnimProxy.png`（`03:06–03:17`） | `03:07` | `03:14` | 3 秒 | `纯 C++ 动画代理` | Boss 动画没有堆复杂蓝图，而是写了自定义 FAnimInstanceProxy，在工作线程直接按服务器时间采样和混合动作。 |
| **22** | `P2_13_AutomationTests.png`（`03:17–03:37`） | `03:18` | `03:24` | 3 秒 | `原生自动化测试` | 半 AI 开发最怕改出隐性回归，所以我把修过的坑都写进了 UE 原生 Automation Test。 |
| **23** | `P2_13_AutomationTests.png` | `03:27` | `03:34` | 3 秒 | `原生自动化测试` | 像这里直接自动验证友军穿射、实墙阻挡和机关校验，目前七项原生测试和双人延迟实测全部通过。 |
| **24** | 片尾卡（`03:37–03:45`） | `03:38` | `03:43` | 尾声 | （无标签） | 以上就是 P2 的全部技术分享，感谢各位面试官观看，期待进一步交流。 |

---

## 三、直接复制发给 ChatGPT（Computer Use）的 P2 全自动执行指令

```markdown
请你使用 Computer Use，全自动完成我的 UE5 项目 `BoldersDemo` 的 **P2 代码与技术解析视频**（先在 JetBrains Rider 截 13 张高清代码图，再打开必剪完成配音、红框标注、字幕与成品导出）。

唯一完整依据文档：`H:\UE5\Unreal Projects\BoldersDemo\Docs\求职Demo视频制作\07_P2代码解析_半AI开发与核心技术_ChatGPT全自动指令.md`
请先读取该文档，然后按以下两步连续自动执行（中途无需停下来问我，除非遇到软件未登录或权限弹窗）：

### 第一步：在 JetBrains Rider 中截取 13 张高清代码图
1. 打开 JetBrains Rider 加载 `H:\UE5\Unreal Projects\BoldersDemo\BoldersDemo.uproject`（或 `.sln`），窗口最大化至 `1920x1080`。
2. 统一外观：`Rider Dark` / `Darcula` 暗色主题，字体 `JetBrains Mono`、**字号 `17pt`**、行高 `1.2`，开启行号显示，关闭杂乱内联提示。
3. 除了第 1 张 `P2_01_AgentsRules.png` 需要保留左侧项目目录树（展开 `Source/BoldersDemo/Private` 下所有一级子目录），其余第 2~13 张全部按 `Ctrl+Shift+F12` 隐藏侧栏和底栏，只留干净代码区；确保目标行全部展开、无蓝色选区、无悬停气泡遮挡。
4. 严格按文档第一节表格中的文件路径与精确起止行号，逐张截取 `1920x1080` 无损 PNG 保存到：
   `H:\UE5\Unreal Projects\BoldersDemo\Saved\DemoVideo\02_RiderScreenshots\`
   （文件名严格一致：`P2_01_AgentsRules.png` ~ `P2_13_AutomationTests.png` 共 13 张）。
5. 截完检查 13 张文件齐全、行号完整清晰、无遮挡后，直接进入第二步。

### 第二步：在必剪（Bcut）剪辑、配音与导出 P2 成品
1. 打开必剪，新建 `16:9` 工程 `BoldersDemo_P2_代码解析`，导入刚才截好的全部 13 张 PNG（`P2_01` ~ `P2_13`）。
2. 按文档第二节的时间轴顺序铺排画面：
   - `00:00–00:06`：6 秒纯深色静态片头卡，居中两行简洁白字：`BoldersDemo P2` / `代码架构与半 AI 开发实践`
   - `00:06–03:37`：按表格标注的停留区间依次摆放 `P2_01` ~ `P2_13`（直接干脆切换或 `0.2s` 淡入淡出，**不要加花哨旋转转场或画中画**）
   - `03:37–03:45`：8 秒纯深色静态片尾卡，居中两行简洁白字：`感谢观看` / `BoldersDemo UE5 C++`
3. **AI 配音与句间停顿（重点）**：
   - 严格按文档第二节的 **01 ~ 24** 共 24 句口播文案逐句生成必剪 AI 配音，音色锁定 **「曼波讲故事」**，语速 **`1.0x`**（正常语速）。
   - 每条语音严格放到表格对应的「开始时间」，保证每两句之间有 **2.5 ~ 3 秒自然停顿**，不连着念；若某条语音实测略长导致离下一句不足 2 秒，顺延后面图片与语音的时间点即可，严禁语音重叠或赶口。
   - 保留同步底部字幕（简洁白字细黑边），位置放在底部居中，避免盖住代码最底行。
4. **简洁红框标注与左上角单标签（重点）**：
   - 在每张代码图上，按文档第一节「必剪高亮红框行号」列，叠加细线空心红色/琥珀金矩形框，准确框住正在讲解的核心代码行。
   - 按文档第二节「左上角单标签」列，在画面左上角放置深底白字简洁小标签；**每次只显示 1 个标签，严格照表写词，禁止自己用 `·`、`•`、`|` 拼接多个词**（片头卡和片尾卡不显示左上角标签）。
5. **混音与导出**：
   - 「曼波讲故事」配音轨 `-4 dB`；在必剪无版权库选一首极安静克制的科技/极简纯音乐 BGM 压低到 `-28 dB`，片尾 `1.5 秒` 平滑淡出。
   - 导出至：`H:\UE5\Unreal Projects\BoldersDemo\Saved\DemoVideo\04_Exports\BoldersDemo_P2_核心代码与架构解析.mp4`（`1920x1080`，`30fps` 或 `60fps`，高码率 10~14 Mbps MP4）。
6. **导出后自检**：
   - 检查 13 张代码文字锐利可读、红框位置准确、左上角每次仅 1 个无加点标签、每句话停顿舒适不挤、字幕不压底行代码、「曼波讲故事」读音自然。如有问题自动回工程修改重导，完成后向我汇报成片时长与文件大小。
```
