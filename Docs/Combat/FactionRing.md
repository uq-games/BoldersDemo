# 战斗地面标记

## 使用与配置

1P 使用带朝向刻痕的青色环，2P 使用有缺口的琥珀色环，敌方使用红色断环。选中时提高亮度。中立、死亡、隐藏角色、未苏醒的石像，以及脚下检测不到地面的角色隐藏指示环。

点击我方角色可选择指令目标，点击地面移动会保留我方选择。点击怪物会选中怪物，若已选我方角色则发出攻击请求。小地图展开时，Esc 优先收起地图。其余情况下，Esc 或本地控制器的 `ClearSelection()` 清除双方视觉选择，已下达的移动或攻击命令继续执行。PIE 中 Esc 也可能被编辑器用于退出游戏，可在独立运行窗口测试。

打开 `BP_Warrior` 或 `BP_EnemyMushroom`，选中继承的 **FactionRing** 组件：

| 配置 | 默认值 / 作用 |
| --- | --- |
| 运行时材质 | `/Game/BoldersDemo/Assets/Combat/Effects/M_CombatFootprint` |
| Player Color / Enemy Color | 蓝色 / 红色 |
| Normal Brightness / Selected Brightness | 0.7 / 3.0 |
| Radius | 战士、弓箭手、法师与蘑菇怪蓝图配置 76 cm（C++ 默认 55 cm），让圆环露出模型脚部；半径对应可见外沿 |
| Projection Depth | 24 cm，投射盒沿地面法线的半深度 |
| Trace Above Feet / Below Feet | 从脚底上方 40 cm 向下检测至脚底下方 160 cm |
| Surface Offset | 沿地面法线抬高 1 cm |
| Stationary Trace Interval | 0.2 秒，仅对静止在固定地面的角色降低检测频率；移动或可移动地面每帧检测 |

脚下圈、攻击距离和危险区分别使用 `M_CombatFootprint`、`M_CombatReach` 和 `M_CombatDanger`。三个材质都是 Deferred Decal，按攻距、身份标记、危险区的顺序由下至上叠放。材质以地表坡度限制投射，减少墙面串色。[Epic 贴花材质文档](https://dev.epicgames.com/documentation/unreal-engine/decal-materials-in-unreal-engine)

Tab 切换玩家攻距。冷色轮廓读取当前攻击模块的范围和角色胶囊半径，冷却期间变暗。法师落雷在合法目标处显示带十字的虚线落点，起手后锁定在判定位置。

守卫用暖色圆形，熔炉骑士用随招式变化的暖色椭圆。边界与伤害判定共用参数，边界内的条纹从中心扩张，前摇结束时到达边界。孢弹投手的落点预警持续到弹体落地。危险区的视觉层级参考了 [Riot 战斗清晰度说明](https://www.leagueoflegends.com/en-us/news/dev/clarity-in-league/) 和 [UE5 范围攻击示例](https://devcol.net/blog/2026-11-18-pseudo-aoe-attacks-server-validated)。

## 代码职责与调用关系

```mermaid
flowchart TD
    A[BaseCharacter 构造函数] --> B[创建并持有 FactionRing 组件]
    C[PlayerController 点击命中] --> D[UpdateSelectedCharacter / UpdateSelectedEnemy]
    D --> E[RefreshOutline 更新旧目标和新目标]
    E --> F[FactionRing.SetSelected]
    F --> G[UpdateAppearance 更新 MID 颜色与亮度]
    H[组件 BeginPlay / 移动后的 Tick] --> I[UpdateRing]
    I --> G
    I --> J[脚底地面射线]
    J --> K[命中位置和法线决定 Decal 世界位置与朝向]
    L[Esc / ClearSelection] --> D
```

- **ABaseCharacter**：继续作为阵营、生命和组件所有权的来源。新增 `GetFaction()` 只读访问器，以及通过 `CreateDefaultSubobject` 创建的 `FactionRing`。原角色 Actor Tick 仍关闭。
- **UFactionRingComponent**：继承 `UDecalComponent`，独立负责圆环表现。`BeginPlay()` 在专用服务器关闭 Tick，在客户端建立移动组件的 Tick 依赖。`UpdateRing()` 在 PostPhysics 阶段读取角色脚底位置、检测地面、调整 Decal，并决定是否可见。
- **UpdateAppearance()**：首次有材质时创建该组件专用的动态材质实例，以 `UPROPERTY(Transient)` 持有，避免 GC 问题。阵营或选中亮度改变时才写入材质参数，不每帧创建 MID。
- **ABoldersPlayerController**：继续保存真正的选择状态。新增 `UpdateSelectedCharacter()` 统一我方选择入口，已有 `UpdateSelectedEnemy()` 管怪物；`RefreshOutline()` 同时刷新原描边与圆环高亮。重新分配、死亡、切换目标及退出控制器时恢复旧目标状态。

圆环不复制选中状态，不增加 RPC；客户端使用原有复制的 Faction、Health 和角色位置独立计算。服务端控制权、寻路、GAS 和伤害逻辑保持原有职责。组件内的选中布尔值只是本地显示缓存，不负责授权。当前与原描边一样面向每个进程一个本地视角，未实现同一世界分屏玩家分别看到不同高亮。

## 地形处理与边界

地面查询只包含 WorldStatic / WorldDynamic，忽略自身胶囊与通常属于 Pawn 的其他角色。脚底来自胶囊中心减去缩放后的半高。用命中法线的反方向作为 Decal 局部 X 投射方向，因此可以贴合斜坡。组件使用绝对世界位置、旋转和缩放，圆环的朝向刻痕随角色转身，大小不受模型缩放影响。

静止检测优化保留组件每帧 Tick，仅在角色脚底、朝向和固定地面均未变化时复用地面结果，默认每 0.2 秒复查。地面为可移动组件、正在下落、位置变化或缓存失效时仍逐帧检测，选中和死亡反馈不受检测间隔影响。优化示例见 [我做的角色圆环地面检测优化](../优化/角色圆环地面检测优化.md)。

贴花只投射到盒范围内、允许接收贴花的表面。脚下圈优先检测复杂碰撞，缺少复杂碰撞时退回简化查询。攻击和危险区从上向下投射，保留原判定的水平半径。两层地面高差超过投射深度时，边界只会落在对应高度内。

材质生成入口为 `Scripts/Editor/Combat/setup_ground_combat_decals.py`。

## 原版验证（2026-11-17）

- Development Editor 构建通过。
- `BoldersDemo.Presentation.FactionRing` 与原 `MonsterOutline` 自动化测试通过。覆盖每角色独立 MID、蓝红参数、选择与取消、平地、20 度斜坡、高台、移动、无地面、死亡及中立隐藏。
- 独立进程读取确认角色蓝图保存了贴花材质。本轮调整后蓝图半径为 76 cm。
- TestMap 的 Client 1 窗口实测蓝/红圆环显示、点击我方和怪物选中、取消与移动跟随。运行时记录确认客户端选中亮度为 3.0，服务器对应角色仍为 0.7，体现本地选择隔离。
- 当前 PIE 的 Esc 由编辑器用于结束预览；取消选择通过同一 `ClearSelection()` 入口实测。未做打包或专用服务器进程验收，也未对复杂连续崎岖地形作逐帧视觉验收。
- 日志和运行记录：`Saved/FactionRing/StateTests.log`、`Tests.log`、`RuntimeSelection.txt`、`RuntimeEnemySelection.txt`、`RuntimeMovement.txt`。测试已结束，编辑器保持打开。
- 收尾时在远程命令回调中关闭临时 Python 连接触发了 PythonScriptPlugin 访问异常。此前 PIE 已正常结束，日志确认内容资产无未保存项；故障日志保留在 `Saved/FactionRing/EditorValidation.log`。重新正常启动编辑器，不再次在活动回调内关闭连接。测试过程没有保存运行时角色位置到地图。

## 本轮验证

Win64 Development 与 Development Editor 构建通过，七项项目原生测试通过。双人 PIE 的两个视角都能区分 1P 和 2P。弓手、法师攻距中央采样与当前模块计算一致，法师落雷的预览和出手范围半径均为 166 cm。单人回放验证了 Tab、第一帧守卫预警和坡道上的骑士跳劈。坡道测试中的椭圆半轴与该次跳劈判定一致，分别为 238 cm 和 286 cm。战士完整打完两波守卫并击败骑士。结果及截图位于 `Saved/GroundCombat`。
