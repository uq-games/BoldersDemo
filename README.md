# BoldersDemo

Developed with Unreal Engine 5

这是一个模仿博德之门战斗系统的极简化demo

`master` 是从 `b7a58d9` 建立的学习起点。打开 `TestMap`，在开局界面确认战士、弓箭手或法师，点击自己的角色，再点击地面移动。三个职业共享基础移动逻辑。

第一项练习可以从“一次普攻恰好扣一次血”开始。先验证点中目标，再验证距离和命中时机，最后用两个玩家检查双方看到的血量。开局界面的调用关系见[职业选择说明](Docs/UI/ClassSelection.md)，已完成的战斗实现保存在[完整参考分支](https://github.com/uq-games/BoldersDemo/tree/codex/reference-full-demo)。

项目目录、资产导入与命名约定见 [项目文件与资产组织规范](Docs/ProjectOrganization.md)。
