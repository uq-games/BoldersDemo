# 给 ChatGPT（Computer Use）的分阶段执行指令包

> **使用方式**：按 **Stage 1 → Stage 2 → Stage 3 → Stage 4 → Stage 5** 的顺序，每次把对应代码块里的完整指令复制发送给开启了 **Computer Use** 的 ChatGPT。每个 Stage 结束后先确认产出文件齐全、画面合格，再发送下一个 Stage。

---

## Stage 1｜环境准备、目录创建与 OBS / UE5 录制配置

```markdown
你现在需要帮我使用 Computer Use 为我的 UE5 求职项目 `BoldersDemo` 录制求职 Demo 视频素材并做后续剪辑。我们先执行 **Stage 1：环境与录屏准备**。

### 项目与输出目录
- UE5 项目根目录：`h:\UE5\Unreal Projects\BoldersDemo`
- 本次视频素材根目录：`h:\UE5\Unreal Projects\BoldersDemo\Saved\DemoVideo`
- 请先确认并创建以下 4 个子目录（如不存在）：
  1. `Saved\DemoVideo\01_RawRecordings`
  2. `Saved\DemoVideo\02_RiderScreenshots`
  3. `Saved\DemoVideo\03_BcutProjects`
  4. `Saved\DemoVideo\04_Exports`

### 任务 1：配置 OBS Studio
1. 打开 OBS Studio。
2. 进入「设置 -> 视频」：
   - 基础（画布）分辨率：`1920x1080`
   - 输出（缩放）分辨率：`1920x1080`
   - 常用 FPS 值：`60`
3. 进入「设置 -> 输出 -> 录像」：
   - 录像路径设为：`h:\UE5\Unreal Projects\BoldersDemo\Saved\DemoVideo\01_RawRecordings`
   - 录像格式：`mp4`（或 `mkv` 并在「高级」里勾选“自动转封装为 mp4”）
   - 视频比特率/质量：硬件编码（NVENC/AMF/QSV 优先），`14000 Kbps` 或 `CQP 18`
4. 进入「设置 -> 热键」：
   - 给「开始录像」和「停止录像」设置热键为 `F9`（避免录像首尾录进鼠标点击 OBS 的画面）。
5. 音频混音器检查：
   - 保留「桌面音频」开启（录游戏原声），**静音「麦克风/Aux」**（防止录进环境杂音）。
6. 场景源设置：
   - 新建场景 `BoldersDemo_Capture`，准备添加「窗口采集」源（等下面打开 UE5 游玩窗口后再绑定到游戏窗口，确保铺满 `1920x1080` 画布、不要拍到任务栏和编辑器边框）。

### 任务 2：启动 UE5 编辑器并检查关卡
1. 如果 UE5 编辑器尚未打开，双击打开 `h:\UE5\Unreal Projects\BoldersDemo\BoldersDemo.uproject`。
2. 确认当前打开的关卡为 `/Game/BoldersDemo/Assets/Maps/L_VolcanoCombat`。
3. 点击顶部工具栏 Play 按钮右侧的三个点（Play Mode 设置）：
   - 选中 `New Editor Window (PIE)` 或 `Standalone Game`
   - 在 `Advanced Settings` 中将 `New Viewport Resolution` 设为 `1920 x 1080`（方便 OBS 无黑边满屏采集），`Number of Players` 先设为 `1`，`Net Mode` 设为 `Play As Standalone`。
4. 启动一次试玩，按 `F9` 录一段 5 秒测试视频后停录，确认：
   - 选角界面三职业 3D 预览正常显示；
   - 画面清晰无调试绿字（若有 `stat fps` 等信息请按 `~` 关闭）；
   - 试播测试录像确认有游戏音效、无杂音、不掉帧，然后删除该测试录像。

完成后向我汇报 Stage 1 检查结果（OBS 参数、UE 窗口状态、测试录像是否正常），等待我发 Stage 2。
```

---

## Stage 2｜P1 实机游玩分段录屏（7 个核心素材 + 可选测试录像）

```markdown
现在执行 **Stage 2：P1 实机游玩录屏**。
请严格对照 `h:\UE5\Unreal Projects\BoldersDemo\Docs\求职Demo视频制作\01_P1游玩实录_分镜与文案.md` 的镜头需求，利用 OBS 热键 `F9` 分段录制以下 7 个原片，录完每一段后立刻在 `Saved\DemoVideo\01_RawRecordings\` 中重命名为指定文件名。

### 游戏操作速查（请牢记）
- 移动：`WASD`（沿固定俯视屏幕方向）
- 攻击：鼠标左键点击敌人或可交互机关
- 切换技能：数字键 `1 / 2 / 3 / 4` 或点击底部技能图标
  - **法师**：`1` 火球（爆炸）、`2` 水浪（挂潮湿+推怪）、`3` 落雷（有地面预览圈+天降雷击，克制弱雷 Boss）、`4` 冰锥（配合潮湿触发冻结）
  - **战士**：`1` 剑盾（三连刀光）、`2` 双手剑（扇形劈砍）、靠近冷却槽按 `F` 蘸刀附魔
  - **弓手**：`1` 普通箭、`2` 弹射箭（一箭三跳）、`3` 爆炸箭、`4` 冰冻箭（瞬冻打断）
- 交互与辅助：`Tab` 开关地面攻距圈、`E` 阅读路牌、`Esc` 收起路牌/大地图、点击右上角小地图展开全岛地图、结算后按 `R` 或点「再战」重开

---

### 分段录制清单（请逐段完成并自检）

1. **`Raw_01_ClassSelection_Mage.mp4`**（约 15~20 秒）
   - 启动游戏停留在选角界面，按 `F9` 开始录像。
   - 鼠标依次点选「战士」停留 2.5 秒 → 点选「弓手」停留 2.5 秒 → 点选「法师」停留 2 秒 → 点击确认按钮进入铸造岛，画面稳定出现角色后等 2 秒按 `F9` 停止。

2. **`Raw_02_Mage_Wave1_And_WaySign.mp4`**（约 45~60 秒）
   - 保持法师在入口平台，按 `F9` 开始录像。
   - 演示 `WASD` 走动 → 按两次 `Tab` 展示地面攻距圈开关（最终保持开启）→ 走到入口路牌旁按 `E` 打开说明停留 2 秒再按 `Esc` 收起 → 点击右上角小地图展开全岛图停留 2 秒再 `Esc` 收起。
   - 向下走到第一波开阔地触发首波守卫（余烬突袭者 + 孢弹投手）：
     - 务必打出一套清晰的元素连招：`2` 水浪挂潮湿 → `4` 冰锥触发冻结 → `1` 火球触发融化！
     - 尽量在孢弹投手蓄力预警时打断它一次；躲开地上红圈预警。
   - 清完首波、看到金色菱形目标指示后停 2 秒按 `F9` 停止。

3. **`Raw_03_Mage_Wave2_And_Hazards.mp4`**（约 50~70 秒）
   - 沿外圈旧铸道走向西侧第二波区域，按 `F9` 开始录像。
   - 触发第二波守卫（含玄武岩卫）：
     - **必须录到的高光 1**：引怪靠近炸药桶，用 `1` 火球点爆炸药桶炸怪；
     - **必须录到的高光 2**：攻击闪电导柱，让相连冷却槽通电感电敌人；
     - **必须录到的高光 3**：躲开玄武岩卫重砸预警红圈，趁其护壳打开收势窗口，用 `3` 落雷（悬停展示地面落点圈再点下）轰杀。
   - 清完第二波并走向坡道检查点（触发回血）后按 `F9` 停止。

4. **`Raw_04_Mage_Boss_FullFight.mp4`**（约 70~100 秒）
   - 走到核心石台坡道下方，按 `F9` 开始录像。
   - 走上石台唤醒 **熔炉骑士 Boss**，完整录下苏醒动画与一阶段交战：
     - 必须展示：躲开重砸、低位旋扫与跳劈（若在坡道上触发跳劈更佳），每次出预警圈立刻反击；
     - 打到 45% 血量时注意录下延迟狂暴衔接（完成当前招式再狂暴蓄势）；
     - 二阶段狂暴后录到火焰砸地/火环特效，再用 `2` 水浪 / `4` 冰锥 + `3` 落雷完成斩杀；
     - 完整录下 Boss 倒地动画与「铸造岛已夺回」结算面板（含耗时/血量显示），停留 3 秒后按 `F9` 停止。
   - *重录标准*：如果不小心阵亡、或者没录到狂暴特效，请点「再战」重录本段。

5. **`Raw_05_Warrior_BladeDip_Highlight.mp4`**（约 25~35 秒）
   - 重新开局选 **战士**，先走到有冷却槽和守卫的战斗区附近，按 `F9` 开始录像。
   - 走到冷却槽边缘出现 `F` 蘸刀图标 → 按 `F` 蘸刀附魔（录清光束与发光剑身）→ 用 `1` 剑盾连续挥砍敌人展示彩色刀光拖尾 → 切 `2` 双手剑横扫一次 → 按 `F9` 停止。

6. **`Raw_06_Archer_Ricochet_Freeze_Highlight.mp4`**（约 25~35 秒）
   - 重新开局选 **弓手**，来到首波守卫前，按 `F9` 开始录像。
   - 切 `2` 弹射箭射击敌人，录到箭矢在多个守卫间弹跳的连线特效 → 切 `4` 冰冻箭瞬间冻住敌人 → 切 `3` 爆炸箭轰击一次 → 按 `F9` 停止。

7. **`Raw_07_Coop_ListenServer_2P.mp4`**（约 30~45 秒）
   - 回到 UE5 编辑器，把 Play Mode 改为：`Number of Players = 2`，`Net Mode = Play As Listen Server`，`New Viewport Resolution = 960 x 540`（或可左右并排放在屏幕上的合适 16:9 尺寸）。
   - 启动双人 PIE，把两个游戏窗口**左右并排紧贴摆放**（左：1P 主机，右：2P 客户端），调整 OBS 采集区域/双窗口采集使左右两屏清晰完整铺满 1920×1080 画布。
   - 按 `F9` 开始录像：
     - 左窗 1P 选战士、右窗 2P 选弓手进入同一关卡；
     - 两人走到第一波或导柱冷却槽处，让 1P 战士站在怪物前排，操作右窗 2P 弓手**隔着 1P 战士穿射后排敌人**（展示箭矢穿透队友且队友不掉血、敌人扣血）；
     - 再操作 2P 弓手射中闪电导柱给冷却槽通电，双端画面同步呈现特效；
     - 停留 2 秒按 `F9` 停止。

8. **（可选加分项）`Raw_08_AutomationTests_Pass.mp4`**（约 8 秒）
   - 在 UE5 编辑器打开 `Tools -> Session Frontend -> Automation`，搜索 `BoldersDemo.`，勾选全部 7 项原生测试。
   - 按 `F9` 录屏并点击 `Start Tests`，录下全部 7 项变成绿色对勾 `Passed` 后停 2 秒按 `F9` 停止。

所有片段录完并重命名后，列出 `Saved\DemoVideo\01_RawRecordings\` 中的文件清单与每段时长，汇报给我后进入 Stage 3。
```

---

## Stage 3｜JetBrains Rider 高清代码截图（15 张精准截屏）

```markdown
现在执行 **Stage 3：在 JetBrains Rider 中截取 P2 所需的 15 张高清代码截图**。
目标输出目录：`h:\UE5\Unreal Projects\BoldersDemo\Saved\DemoVideo\02_RiderScreenshots\`

### Rider 界面统一配置（截图前务必先调好！）
1. 在 JetBrains Rider 中打开 `h:\UE5\Unreal Projects\BoldersDemo\BoldersDemo.uproject`（或 `BoldersDemo.sln`）。
2. 将 Rider 窗口最大化至 `1920×1080`（若显示器高于 1080p，请确保最终导出/裁切的截图为清晰的 16:9 画面，文字无缩放模糊）。
3. 统一外观设置（`Ctrl+Alt+S` 打开 Settings）：
   - 主题：`Rider Dark` 或 `Darcula`（暗色代码配字幕最耐看）；
   - `Editor -> Font`：字体选 `JetBrains Mono`（或 `Cascadia Code`），**字号调为 `17`（关键！确保小窗播放代码清晰可读）**，行高 `1.2`；
   - `Editor -> General -> Appearance`：确保开启 `Show line numbers`（显示行号），关闭 `Show code lens / inlay hints` 过于杂乱的提示（若不影响可读可保留简短类型提示）。
4. 每次截图前：
   - 点一下空白处避免大块蓝色选区遮挡代码；
   - 除了 `Code_01` 需要显示左侧目录树，其余 `Code_02 ~ Code_15` 按 `Ctrl+Shift+F12` 隐藏所有工具窗口（左侧项目树、底部 Build/Terminal、右侧栏全部收起），只留干净的顶部标签页和纯代码区！
   - 确认所有涉及代码行均**未折叠**（`Ctrl+Shift+Plus` 展开当前文件）。

### 15 张精准截图清单（逐张按路径与行号滚动后截 PNG）
跳转文件快捷键：`Ctrl+Shift+N`；跳转指定行快捷键：`Ctrl+G`。

| 序号 | 保存文件名 | 打开的文件路径 | 屏幕内必须完整显示的行号范围 | 画面特别要求 |
| --- | --- | --- | --- | --- |
| 1 | `Code_01_ProjectStructure.png` | `Docs/CodeGuide.md` | `L1–L30` | **唯一保留左侧 Project/File System 栏的一张**：展开 `Source/BoldersDemo/Private` 下所有一级职责子目录（`AI/Camera/Characters/Combat/Editor/Game/Player/Tests/UI`） |
| 2 | `Code_02_AttackLoadout.png` | `Source/BoldersDemo/Public/Combat/Data/AttackLoadout.h` | `L14–L56` | 隐藏侧栏，完整展示 `EDamageElement`、`EAttackPattern` 与 `FAttackModule` 核心配置字段 |
| 3 | `Code_03_CombatPrecheck_Buffer.png` | `Source/BoldersDemo/Private/Combat/CharacterCombatComponent.cpp` | `L161–L204` | 完整展示 `RequestAttack`（机关元素校验、直控攻距视线、0.44s 预输入缓冲） |
| 4 | `Code_04_MovementWindupLock.png` | `Source/BoldersDemo/Private/Characters/BoldersCombatMovementComponent.cpp` | `L1–L14` | 完整展示整个 `ConstrainInputAcceleration` 文件 |
| 5 | `Code_05_BasicAttackAbility_Start.png` | `Source/BoldersDemo/Private/Combat/Abilities/BasicAttackAbility.cpp` | `L85–L136` | 完整展示 `StartMontage`（安全枪口预检、动态 `StartTime` 计算、双 AbilityTask） |
| 6 | `Code_06_BasicAttackAbility_Muzzle.png` | `Source/BoldersDemo/Private/Combat/Abilities/BasicAttackAbility.cpp` | `L171–L219` | 完整展示弹道生成与 `GetSafeMuzzle` 球体扫掠防穿墙 |
| 7 | `Code_07_DamageExecution_GAS.png` | `Source/BoldersDemo/Private/Combat/Effects/BoldersDamageEffect.cpp` | `L46–L81` | 完整展示 `UBoldersDamageExecution` 构造捕获与 `Execute_Implementation` |
| 8 | `Code_08_ElementalReactionTable.png` | `Source/BoldersDemo/Private/Combat/Attacks/ElementalHit.cpp` | `L20–L60` | 完整展示 `FReactionRule`、9 条 `Rules[]` 表与 `Conduct` 开头 |
| 9 | `Code_09_ElementalApplyHit.png` | `Source/BoldersDemo/Private/Combat/Attacks/ElementalHit.cpp` | `L75–L111` | 完整展示 `ApplyHit` 反应结算、`FElementalHitBatch` 与清除附着 |
| 10 | `Code_10_Projectile_Overlap_Bounce.png` | `Source/BoldersDemo/Private/Combat/Attacks/AttackProjectile.cpp` | `L163–L204` | 完整展示 `Bounce` 弹射三跳与 `OnPawnOverlap` 敌我过滤 |
| 11 | `Code_11_VolcanoHazard_State.png` | `Source/BoldersDemo/Private/Combat/Environment/VolcanoHazard.cpp` | `L296–L342` | 完整展示 `ReceiveAttack` 与 `ApplyToPool` 机关状态流转 |
| 12 | `Code_12_BladeCoating.png` | `Source/BoldersDemo/Private/Combat/BladeCoating.cpp` | `L55–L104` | 完整展示 `TryDipBlade`、`GetEffectiveAttackElement` 与 `ClearBladeCoating` |
| 13 | `Code_13_Boss_Leap_Enrage.png` | `Source/BoldersDemo/Private/Characters/StatueBoss/StatueBossCharacter.cpp` | `L467–L516` | 完整展示 `AdvanceLeap` 斜坡贴地推进与 `BeginAttack` 按帧反推 `HitAfter` |
| 14 | `Code_14_Boss_AnimProxy.png` | `Source/BoldersDemo/Private/Characters/StatueBoss/StatueBossAnimInstance.cpp` | `L10–L52` | 完整展示自定义 `FStatueBossAnimProxy` 工作线程采样与双姿态混合 |
| 15 | `Code_15_Encounter_And_Tests.png` | `Source/BoldersDemo/Private/Tests/Combat/VolcanoReleaseTest.cpp` | `L39–L80` | 完整展示 `FVolcanoProjectileResponseTest`（验证友军不挡视线/穿射/实墙阻挡） |

### 截图后自检
1. 检查 `Saved\DemoVideo\02_RiderScreenshots\` 下 15 张 PNG 是否全部存在、命名一字不差。
2. 抽查确认每张图字号清晰、目标起止行号都在画面内且未被弹窗/光标气泡遮挡。
完成后向我汇报，等待进入 Stage 4（必剪剪辑与配音）。
```

---

## Stage 4｜必剪（Bcut）全流程剪辑、AI 配音「曼波讲故事」与成品导出

```markdown
现在执行 **Stage 4：使用必剪（Bcut）剪辑并导出 P1 和 P2 两个求职 Demo 成品**。
所有文案严格读取我项目中的两份脚本文件，禁止自行删改核心技术数据：
- P1 文案与分镜：`h:\UE5\Unreal Projects\BoldersDemo\Docs\求职Demo视频制作\01_P1游玩实录_分镜与文案.md`
- P2 文案与分镜：`h:\UE5\Unreal Projects\BoldersDemo\Docs\求职Demo视频制作\02_P2代码解析_分镜文案与Rider截图清单.md`

### 全局 AI 配音与字幕规范（P1 和 P2 通用）
1. **必剪 AI 配音音色**：必须选择 **「曼波讲故事」**！
2. **语速**：设为 `1.08x`（如必剪不支持小数步进，设为 `1.1x`），音量 `0 dB`（或 `-3 dB`）。
3. **逐段生成**：按文案中的分段标题（P1 共 8 段：`P1-01 ~ P1-08`；P2 共 10 段：`P2-00 ~ P2-10`），逐段新建文本 → 点击「朗读 / AI 配音」→ 选中 **「曼波讲故事」** 生成音频轨，并同步生成/保留底部字幕。
4. **字幕样式**：底部居中，黑底半透明背景条或黑色描边白字，单行不超过 22 字自动换行，位置略微上移，**严禁遮挡游戏底部技能栏（P1）或代码最底行（P2）**。
5. **音频平衡**：
   - AI 配音（曼波讲故事）：主声道清晰突出（`-3 dB ~ -5 dB`）；
   - P1 游戏原声音轨：压低至 `-15 dB`（Boss 吼叫、落雷、爆炸瞬间可局部留到 `-10 dB`）；
   - 背景音乐（在必剪内置无版权音乐库选一首低存在感的“史诗/冒险”风给 P1，“科技/极简纯音乐”给 P2）：音量压低至 **`-26 dB ~ -28 dB`**，绝不抢人声。

---

### Part A：剪辑并导出 P1（实机游玩演示，目标约 2 分 45 秒 ~ 3 分 00 秒）
1. 在必剪新建 16:9 工程，命名为 `BoldersDemo_P1`。
2. 导入 `Saved\DemoVideo\01_RawRecordings\` 中的 `Raw_01` ~ `Raw_07`。
3. 严格按 `01_P1游玩实录_分镜与文案.md` 的 8 个镜号（`P1-01 ~ P1-08`）编排主视频轨：
   - **P1-01**：开头先切 3 秒 `Raw_04` Boss 狂暴火焰砸地/落雷最燃片段做钩子，再接 `Raw_01` 选角预览；
   - **P1-02 ~ P1-06**：依次接法师实机移动/UI、首波元素反应、二波炸药桶+导柱+玄武岩卫、Boss 一阶段+狂暴+击杀结算；剪掉纯赶路发呆的冗余片段，赶路过渡可给 `1.3x ~ 1.4x` 变速，冻结/融化/击杀瞬间可给 `0.8x` 慢放 1 秒；
   - **P1-07**：接 `Raw_05`（战士按 F 蘸刀+刀光）和 `Raw_06`（弓手弹射箭+冰冻箭）各约 9 秒，中间用 0.3 秒利落转场；
   - **P1-08**：接 `Raw_07`（双人联机同屏），在左半屏顶部加标签 `1P 主机（战士）`，右半屏顶部加标签 `2P 客户端（弓手）`；片尾加 2.5 秒深色卡片：`P2 预告：GAS 元素反应 · 斜坡跳劈 Boss · 双人联机架构源码解析`。
4. 按 `P1-01 ~ P1-08` 对齐导入 8 段「曼波讲故事」AI 配音和底部字幕，并为每段在**左上角**添加 `01_P1游玩实录_分镜与文案.md` 中列出的简洁花字标签（白字深底圆角条）。
5. 导出成品：
   - 路径：`h:\UE5\Unreal Projects\BoldersDemo\Saved\DemoVideo\04_Exports\BoldersDemo_P1_实机游玩演示.mp4`
   - 参数：`1920x1080`、`60 fps`、高码率（推荐 12~16 Mbps）、MP4。

---

### Part B：剪辑并导出 P2（核心代码与架构解析，目标约 4 分 40 秒 ~ 5 分 00 秒）
1. 在必剪新建 16:9 工程，命名为 `BoldersDemo_P2`。
2. 导入 `Saved\DemoVideo\02_RiderScreenshots\` 中的全部 15 张 PNG（`Code_01 ~ Code_15`），同时导入 `Raw_02 / Raw_04 / Raw_05 / Raw_07`（以及如有录制的 `Raw_08`）用于右下角画中画实机对照。
3. 严格按 `02_P2代码解析_分镜文案与Rider截图清单.md` 的 10 段配音（`P2-00 ~ P2-10`）先生成全部「曼波讲故事」AI 配音条并顺序排在音频轨，段间留 `0.4s` 自然停顿，**再让每一张代码截图的时长严格贴合对应配音段**：
   - `P2-00` → `Code_01_ProjectStructure.png`
   - `P2-01` → `Code_02_AttackLoadout.png`
   - `P2-02` → `Code_03_CombatPrecheck_Buffer.png`
   - `P2-03` → `Code_04_MovementWindupLock.png`
   - `P2-04` → 前半段 `Code_05_BasicAttackAbility_Start.png`，讲到“安全枪口检测”时切 `Code_06_BasicAttackAbility_Muzzle.png`
   - `P2-05` → `Code_07_DamageExecution_GAS.png`
   - `P2-06` → 前半段 `Code_08_ElementalReactionTable.png`，讲到“在结算阶段”时切 `Code_09_ElementalApplyHit.png`
   - `P2-07` → `Code_10_Projectile_Overlap_Bounce.png`（右下角叠 25% 大小静音画中画：`Raw_07` 穿队友射击片段）
   - `P2-08` → 前半段 `Code_11_VolcanoHazard_State.png`，讲到“与之呼应的是战士的蘸刀附魔”时切 `Code_12_BladeCoating.png`（右下角叠 `Raw_05` 蘸刀片段）
   - `P2-09` → 前半段 `Code_13_Boss_Leap_Enrage.png`（右下角叠 `Raw_04` Boss 跳劈片段），讲到“动画层面我编写了自定义的动画代理”时切 `Code_14_Boss_AnimProxy.png`
   - `P2-10` → `Code_15_Encounter_And_Tests.png`（如有 `Raw_08` 测试绿勾录像，后半段切 `Raw_08` 全屏或画中画展示 7 项 PASS）
4. **视觉动态与高亮标注（关键！）**：
   - 给每张超过 12 秒的代码截图加轻微关键帧缓推（`100% -> 105%` 缩放），避免死板；
   - 按 `02_P2代码解析_分镜文案与Rider截图清单.md` 表格中的「必剪红框/高亮标注点」，在必剪「贴纸/形状」中叠加细线高亮框（红色或琥珀金空心矩形，线宽适中）精准框住正在讲的函数/行区间；
   - 在顶部放一条常驻章节标题条（当前章节高亮显示，如 `Ch4 · GAS 伤害执行与 9 种元素反应`）；
   - 在讲到关键实测指标时，在右上角弹出简短角标：`前摇滑步 0 cm`、`预警误差 < 10 ms`、`7 项自动化测试 PASS`、`100 ms 延迟双人通过`。
5. 导出成品：
   - 路径：`h:\UE5\Unreal Projects\BoldersDemo\Saved\DemoVideo\04_Exports\BoldersDemo_P2_核心代码与架构解析.mp4`
   - 参数：`1920x1080`、`30 fps` 或 `60 fps`、高码率（推荐 10~14 Mbps，确保代码文字锐利不发糊）、MP4。

导出完成后汇报两个文件的体积、精确时长与试听情况，等待 Stage 5 终检。
```

---

## Stage 5｜成片质量自检与局部返工指令

```markdown
现在执行 **Stage 5：成品质量终检**。请对以下两份成品做逐项核验：
- `Saved\DemoVideo\04_Exports\BoldersDemo_P1_实机游玩演示.mp4`
- `Saved\DemoVideo\04_Exports\BoldersDemo_P2_核心代码与架构解析.mp4`

### 必查 7 项清单
1. **视频参数**：用播放器或 `ffprobe` 检查两者均为 `1920x1080` MP4，无黑边、无花屏、无音画不同步。
2. **时长控制**：P1 在 `2:30 ~ 3:10` 之间，P2 在 `4:15 ~ 5:15` 之间（若超时过多，收紧段间空白或微调语速至 `1.12x`）。
3. **「曼波讲故事」读音排查**：抽听包含 `UE5`、`C++`、`W A S D`、`Boss`、`R 键`、`F 键` 的句子，若有吞字或怪读音，仅修改该句的配音触发文本（保留正确字幕）重新生成该单段语音。
4. **字幕防遮挡**：检查 P1 底部字幕是否压住技能栏图标、P2 底部字幕是否压住代码最底行；若有遮挡，统一把字幕 Y 轴调整到安全区域或稍微缩小 1~2 号字。
5. **代码可读性**：在 50% 缩放窗口下预览 P2，确认 `Code_01 ~ Code_15` 每张截图的字都清晰可辨、高亮红框准确框中对应代码行。
6. **混音听感**：确认 P1 战斗音效不会盖过「曼波讲故事」人声，BGM 全程柔和不刺耳，结尾有 1.5 秒平滑淡出（Fade Out）不戛然而止。
7. **无穿帮画面**：首尾没有录进 OBS 窗口、桌面任务栏、Windows 通知弹窗或 Rider 报错红波浪线。

请输出一份简洁的《成片质检报告》，列出 7 项检查结果；若有任意一项未通过，请直接回到必剪对应工程完成返工并重新导出覆盖，直到 7 项全绿！
```
