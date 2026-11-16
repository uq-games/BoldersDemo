# 骑士雕塑 Boss：低位持斧与重型动作 V2

更新日期：2026-11-16；Unreal Engine 5.9。

## 使用入口

预览关卡：`/Game/BoldersDemo/Assets/Maps/L_StatueBoss_Preview`，模拟运行可查看四段循环动作。
蓝图：`/Game/BoldersDemo/Blueprints/Characters/StatueBoss/BP_StatueBoss`，拖入场景默认待机。
资产目录：`/Game/BoldersDemo/Assets/Characters/StatueBoss/`。

`SK_StatueBoss` 只包含人物；`SM_StatueBoss_Axe` 是独立斧头。蓝图的 Axe 静态网格组件挂在人物组件的 `AxeGrip` Socket，Socket 属于 `hand_r`，挂接偏移与旋转均为零。新生成实例也会自动挂接。21 根骨骼保留兼容性，旧 weapon 骨骼不再绑定任何顶点。右肩甲重新适配低位手臂。

## 四段动作

本版四段动作均重新制作，正常低位持斧，不举斧过头。

| 动作 | 时长 | 表现 |
| --- | --- | --- |
| Idle | 6.4 秒 | 缓慢呼吸和轻微重心变化，手臂自然下垂 |
| Walk | 5.2 秒 | 原地缓步，长支撑、重心左右转移，低位小幅摆斧 |
| GroundAttack | 5.2 秒 | 低位向后蓄力，快速下砸、身体下沉，停顿后缓慢收势 |
| Stomp | 4.8 秒 | 先转移重心，慢抬右腿，高位停顿，快速踩下，头朝地面看 |

踩踏抬腿约 0.45–1.85 秒，高位停顿至 2.2 秒，2.2–2.4 秒加速落脚，随后沉身、停顿和收势。脚踝相对起始位置最高抬升约 1.9 米。地面攻击落点约 2.18 秒，踩踏约 2.4 秒，尚未添加伤害 Notify。

人物约 6 米高，导入统一厘米，动画 30 FPS，无水平根位移；保留脚底贴地用的垂直根变化。走路位移需由角色移动逻辑驱动。

## 来源和边界

模型来源为项目已有 Inferno World Free 中的 SM_StatueKnight_002 和 SM_Axe_001，遵循原包授权。原包资产与演示地图未替换。项目材质 M_StatueBoss 引用原包纹理。

上一版下载的 Gamma Studio Free Animation Pack 保留在 `/Game/SampleAnimationPack/`，本版四段动画是重新制作的动作，不再使用上一版待机、走路重定向结果。

BP 基于 SkeletalMeshActor，使用 Single Animation 数据；本次为骨骼、动画、持斧和预览交付，未接入战斗 AI、生命值、伤害、状态机、网络或 GAS。预览碰撞关闭；未验收布娃娃和完整游戏打包。

## 可编辑源文件

当前重导入源文件位于 `H:/UE5/AssetSources/BoldersDemo/StatueBoss/V2/`：StatueBoss.blend、StatueBoss_UE.blend、人物和独立斧头 FBX、四段动画 FBX。
`build_heavy.py` 从上级 rigged.blend 制作本版；`export_assets.py` 导出厘米单位资产，依赖 Blender Python bpy 5.3。执行会重写对应输出，应先保留手工修改。

本版编辑前资产快照在项目 `Saved/BossRigV2/Before/`。本版素材另存为 `H:/UE5/AssetSources/Backups/StatueBoss_20261116_V2.zip`（同盘快照）。

## 验证

V2Validation 目录保存骨骼采样、Socket 挂接和运行采样结果。已检查四段动画共用 Skeleton、根缩放为 1、无水平根漂移、未缩小；检查新生成蓝图的 Socket 挂接与预览实例。已查看 Unreal 模拟画面及制作场景关键姿势。
