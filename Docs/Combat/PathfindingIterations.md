# 寻路方案迭代记录

记录日期：2026-11-17。这里的版本是方案阶段，不对应独立 Git 标签。当前实现见 `AttackPositioningComponent`、`AttackPositionQuery`、`AttackPositionRules` 和 `BoldersAIController`。

| 阶段 | 实现与目的 | 观察到的问题 | 后续处理 |
| --- | --- | --- | --- |
| 1. 定时追击 | 周期检查攻击距离，未满足时继续接近目标，满足后播放攻击 | 查询、移动、战斗集中，反复更新追击逻辑；没有独立选择可站立攻击点 | 分离战斗与站位职责，引入 EQS |
| 2. EQS 选攻击点 | 在目标周围采样，过滤攻击距离、视线、站立空间与路径可达性；选一个点交给 AIController | 能到点不代表路线不会贴着目标穿过；碰撞可能导致卡住和绕行 | 加入针对目标胶囊的路径线段净空过滤 |
| 3. 职责拆分与路径过滤 | Combat 管攻击意图、Montage、命中；AttackPositioning 管查询与移动；Rules 共用几何判断；AttackPath 检查完整路径并按路径长度评分 | 最终目的地仍是精确坐标，避让偏离后可能回绕；部分位置过于拥挤 | 调整站位与胶囊尺寸，并继续做轨迹对比 |
| 4. 胶囊与采样范围调整 | 战士半径 26、蘑菇半径 28；2 圈 × 12 点，半径为攻击有效中心距离减 40～25；到达容差 10 | 缩小胶囊只改变碰撞体积，不能解决“已经能攻击还必须走到点” | 改变接近完成条件 |
| 5. 满足攻击条件提前结束 | 攻击移动期间监听 OnCharacterMovementUpdated；服务器检查地面状态、距离、视线，满足就中止自己的路径并通知 Combat；停止范围向内留 5 | 解决进入攻击范围后的回绕，但接近末段仍会被 RVO 横向调整 | 单独对比 RVO 开关及检测范围 |
| 6. 收紧 RVO 范围（当前） | 保留 RVO，把 AvoidanceConsiderationRadius 从 500 改成 150；保存原生默认值、角色蓝图与现有实例 | 同路线额外末段侧移减少；不是所有动态拥堵问题的完整解法 | 后续以实际回合制占位需求决定是否继续保留 RVO |

## 当前流程

点击敌人 → 服务器验证攻击命令 → 已能攻击则直接攻击，否则运行一次 EQS → AIController 根据 NavMesh 寻路 → CharacterMovement 移动，RVO 可局部修正速度 → 已满足攻击条件或路径完成 → Combat 再检查并攻击。

EQS 选点，NavMesh 提供导航空间和路径，RVO 局部避让其他角色。这三个职责独立。候选点没有碰撞，不会把经过它的角色推开。角色当前不参与导航网格生成；关闭 RVO 后，其他角色仍有胶囊碰撞，但路径规划不会自动绕开他们的当前位置。

## 验证与边界

- `Saved/MovementDiagnosis/Comparison.json`：早期 RVO 开关对比，曾记录进入攻击范围后绕回终点；只“最终命中”不足以证明没有绕行。
- `Saved/ApproachFix/Check.json`：提前停止后，攻击期间位置采样偏移为 0。取消命令、没有站位的回归见同目录 EdgeCases.json。
- `Saved/OuterApproach/Compare.json`：同一个 EQS 点，500 范围的末段从约 (594,-343) 偏到 (624,-300)，150 与关闭 RVO 的末段均到约 (638,-314)。前段绕障路线仍存在，不能把所有转弯归因于 RVO。
- `Saved/OuterApproach/Blocker.json`：加入挡路角色仍可通过并命中。重开编辑器后确认运行角色范围为 150。

上述是小规模双人 PIE 定向测试，后台采样不能当性能基准。没有覆盖密集人群、快速对向移动、所有台阶和所有动态目标。EQS 有限采样可能找不到实际存在的站位；目标移动后当前不会自动反复查询。回合制还可加入单位占位、行动点路径预算和目标移动后的事件重规划。

## 参考

- [Epic EQS Quick Start](https://dev.epicgames.com/documentation/unreal-engine/environment-query-system-quick-start-in-unreal-engine)：将攻击和移动站位作为可切换行为。
- [Epic Avoidance](https://dev.epicgames.com/documentation/unreal-engine/using-avoidance-with-the-navigation-system-in-unreal-engine)：RVO、Detour Crowd 的职责与区别；RVO 不依赖 NavMesh。
- [OnCharacterMovementUpdated](https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Character/OnCharacterMovementUpdated)：移动更新完成后的事件。
