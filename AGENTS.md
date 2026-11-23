# BoldersDemo 项目资产与文件放置规则

在本项目中新建、导入或整理文件时遵守以下约定。目录中的功能或角色子目录按实际需要创建；详细说明见 `Docs/ProjectOrganization.md`。

## 项目自有的 Unreal 资产

项目自有内容统一放在 `Content/BoldersDemo/`（内容浏览器中的 `/Game/BoldersDemo/`）。先区分 `Assets` 和 `Blueprints`，再在各自目录下按功能或角色分类：

| 内容 | 放置位置 | 示例 |
| --- | --- | --- |
| 角色蓝图、动画蓝图 | `Content/BoldersDemo/Blueprints/Characters/<角色>/` | `BP_Hero`、`ABP_Hero` |
| 战斗、回合、技能等逻辑蓝图 | `Content/BoldersDemo/Blueprints/Combat/` | 按需要再细分功能 |
| UI 控件蓝图 | `Content/BoldersDemo/Blueprints/UI/` | `WBP_CombatHUD` |
| 多个功能共用的蓝图 | `Content/BoldersDemo/Blueprints/Shared/` | 公共蓝图 |
| 项目地图和共用测试地图 | `Content/BoldersDemo/Assets/Maps/` | `TestMap` |
| 自有角色网格体、动画序列、材质、纹理和专属数据 | `Content/BoldersDemo/Assets/Characters/<角色>/` | 相关非蓝图资产按角色聚合 |
| 战斗、技能等数据资产实例 | `Content/BoldersDemo/Assets/Combat/` | `DA_Ability_Fireball` |
| UI 贴图等非蓝图素材 | `Content/BoldersDemo/Assets/UI/` | 按界面细分 |
| 输入配置资产 | `Content/BoldersDemo/Assets/Input/` | Input Action、Input Mapping Context |
| 多个功能共用的非蓝图资产 | `Content/BoldersDemo/Assets/Shared/` | 公共材质等 |

其他自有非蓝图资产，例如音频、特效、材质和动画，放在 `Assets/` 下对应功能或对象的目录；相应的蓝图类放在 `Blueprints/` 下对应功能目录。蓝图类与数据资产实例要区分：例如自定义数据资产的蓝图类放 `Blueprints/`，实例放 `Assets/`。

个人临时试验内容放在 `Content/Developers/<用户名>/`。文件夹使用简短英文名，自有资产使用一致的类型前缀，例如 `BP_`、`ABP_`、`WBP_`、`DA_`、`SM_`、`M_`、`MI_`、`T_`。

## 外部导入内容

- 第三方资产包按来源或包名放在 `Content/` 下的独立顶层目录，与 `Content/BoldersDemo/` 并列，保留包内结构。例如 `Content/Inferno_World_Free/`、`Content/ModularRPGHeroesPBR/`。
- 项目自建的子蓝图、材质实例、数据和适配内容放在项目自有目录，并可直接引用第三方资产。
- 使用编辑器的 Migrate 导入其他 UE 项目的资产，检查依赖和目标路径。Fab 资产包及插件遵循各自安装、更新与路径要求。第三方 UE 插件放在项目或引擎的 `Plugins/`，C++ 外部库按模块或插件集成。
- 原始 FBX、PNG、WAV 等源文件根据重导入需要保存在有备份的素材库并记录来源；导入后的 Unreal 资产按上述目录规则放置。

## C++ 与其他项目文件

- C++ 代码放在 `Source/BoldersDemo/`。需要提供给其他模块的头文件放 `Public/`，模块内部头文件和 `.cpp` 放 `Private/`；按 `Characters`、`Combat` 等职责建立对应子目录。
- 项目配置放 `Config/`，项目文档放 `Docs/`；继续使用现有模块，确有需求再新增模块或插件。

## 移动与引用

- 移动或重命名 `.uasset`、`.umap` 时使用 Unreal 编辑器或其资产管理接口，核对引用、Redirectors、配置及硬编码路径，并保存受影响资产。
- 移动地图后检查游戏默认地图和编辑器启动地图配置，验证新路径可加载。
