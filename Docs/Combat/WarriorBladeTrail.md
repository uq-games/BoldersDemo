# 战士刀光拖尾

旧刀光在 `AttackHit` 时生成一张朝向目标的平面。它依据角色位置和攻击范围摆放，所以动画转身、抬手或换武器时，轮廓容易与剑分离；整张平面也会按统一时长留在场上。

这里采用 Epic [动画通知](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/animation-notifies-in-unreal-engine)中的两个附着点和通知时间窗，以及 [Niagara Ribbon](https://dev.epicgames.com/documentation/unreal-engine/how-to-create-a-ribbon-effect-in-niagara-for-unreal-engine?lang=zh-CN)中的连续条带、每个采样点独立寿命的做法。项目里用 C++ 生成条带，直接读取当前武器网格的变换。

- 蒙太奇 `Blade VFX` → `UAnimNotifyState_WeaponTrail` → `AWeapon` → `AWarriorBladeTrail`。开始时取当前装备剑，动画姿势更新后把刀根、刀尖换算成世界坐标。历史点留在经过的位置；低帧率时在两次姿势之间插值，位移异常时断开条带。
- 通知结束或武器换网格时停止采样，旧点按年龄淡出。材质 `M_WarriorBladeTrail` 用顶点 Alpha 控制衰减，局部齿线与刀尖亮边由 UV 控制；突刺只取刀尖附近的窄段。颜色由每次挥剑的动态材质实例设置。
- 只有挥剑或残影消散时才更新网格。条带最多 48 组采样，复用同一个网格节，关闭碰撞和阴影。显示端基于各自播放的蒙太奇生成视觉效果，不同步条带顶点；原来的 `AttackHit` 仍负责服务器伤害。

在五个 `AM_Warrior_*` 中选中 `Blade VFX` 轨的 `Weapon Trail`，拖动通知两端可改起止时间。`Fade Time` 改每段残影的寿命，`Tip Width` 改亮边宽度，`Tint` 改颜色；更换剑模型后，按模型的本地坐标重新对齐 `Blade Root` 和 `Blade Tip`。`Scripts/Editor/Combat/Elemental/setup_warrior_blade_trail.py` 可重建本次配置，会覆盖手调后的刀光通知和材质节点。

UE 5.9 Editor 构建通过。TestMap 渲染运行通过五招伤害、刀光生成和消散检查；劈砍途中切换双手剑时，没有造成伤害，尾迹也已清除。双人 PIE 由客户端发起双手剑横扫，主机与客户端各生成和清除一条尾迹，双方目标均减少 33 点生命。
