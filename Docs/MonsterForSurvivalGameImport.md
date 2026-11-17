# Monster for Survival Game 导入记录

- 日期：2026-11-17。
- 来源：用户 Fab 库，Dungeon Mason 的 [Monster for Survival Game PBR Polyart](https://www.fab.com/listings/9fdd1645-600d-460f-be4b-0b6421b7cf38)。使用授权以用户账户中该资产的实际许可记录为准。
- 安装方式：Epic Games Launcher → 添加到工程 → 显示所有工程 → BoldersDemo → 选择资产版本 5.8。目标项目继续使用 UE 5.9，未修改 EngineAssociation。
- 位置：`Content/MonsterForSurvivalGame/`，保留作者原始目录和资产名称，未编辑原件。
- 内容：8 类怪物，PBR、Polyart 两套，共 16 个骨骼网格体、16 个骨骼、16 个物理资产、280 个动画序列；含材质、纹理及原包演示地图，总计 356 个资产。
- UE 5.9.0（CL 60351404）无界面加载检查通过：356 个资产均可加载；模型骨骼和材质槽有效；动画长度大于零，动画骨骼有对应模型。进程退出码为 0。
- 检查证据：`Saved/MonsterImport20261117/Validation.json` 和 `Validation.log`。检查未包含逐动作画面验收或打包运行。
- 项目 Config 文件与导入前哈希一致，默认地图未修改；未创建项目逻辑蓝图，未修改 ElyGambit。
