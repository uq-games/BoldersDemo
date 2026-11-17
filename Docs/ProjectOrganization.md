# BoldersDemo 项目文件与资产组织规范

> 本规范适用于项目后续新增、导入和整理的内容；示例目录按实际需要创建。既有资产的移动应单独检查引用和配置。

## 1. 目标与基本原则

- 让项目自己的功能、第三方素材和 C++ 源码各有明确位置，方便查找和后续维护。
- 项目自有 UE 内容先分为 `Assets` 与 `Blueprints`，再在各自目录内按功能或对象归类。
- 第三方资产包保留各自的顶层目录，与 `BoldersDemo` 并列；保留原包的结构和来源边界。
- 不因统一目录而提前创建空目录，也不为简单 Demo 引入额外模块或 Lyra 的整套插件架构。

## 2. 项目目录

```text
BoldersDemo/
├─ Content/                         # 编辑器中的 /Game
│  ├─ BoldersDemo/                  # 项目自己创建或维护的 UE 内容
│  │  ├─ Assets/                    # 项目自有的非蓝图 UE 资产
│  │  │  ├─ Maps/                   # 项目地图
│  │  │  ├─ Characters/             # 自有角色美术、动画等
│  │  │  ├─ Combat/                 # 战斗相关数据资产等
│  │  │  ├─ UI/                     # 界面素材
│  │  │  ├─ Input/                  # 输入配置资产
│  │  │  └─ Shared/                 # 多个功能共用的非蓝图资产
│  │  └─ Blueprints/                # 项目自有蓝图，按功能细分
│  │     ├─ Characters/             # 角色及动画蓝图
│  │     ├─ Abilities/              # Gameplay Ability 蓝图
│  │     ├─ Combat/                 # 战斗逻辑蓝图
│  │     ├─ UI/                     # 控件蓝图
│  │     └─ Shared/                 # 多个功能共用的蓝图
│  ├─ Inferno_World_Free/           # 已导入的第三方资产包
│  ├─ ModularRPGHeroesPBR/          # 已导入的第三方资产包
│  ├─ Provider_PackName/            # 其他第三方资产包示例
│  └─ Developers/                   # UE 开发者目录；个人试验内容
├─ Source/
│  └─ BoldersDemo/
│     ├─ Public/                    # 需要暴露给其他模块的头文件
│     ├─ Private/                   # .cpp 与模块内部头文件
│     └─ BoldersDemo.Build.cs
├─ Plugins/                         # 仅在使用项目级插件时出现
├─ Config/                          # UE 项目配置
└─ Docs/                            # 设计、说明和项目规范
```

这是一张组织示意图，除已存在的目录外，其余子目录按需创建。项目专属内容与第三方资产包的顶层目录并列，可降低与新导入内容同名冲突的概率，但不能保证完全无冲突。`Developers` 是 UE 自带的特殊开发者目录，不纳入项目自有内容目录。

### 如何决定放哪里

| 新内容 | 建议位置 | 说明 |
| --- | --- | --- |
| 角色蓝图 `BP_Hero`、动画蓝图 `ABP_Hero` | `Content/BoldersDemo/Blueprints/Characters/Hero/` | 按角色或功能细分 |
| Gameplay Ability 蓝图 | `Content/BoldersDemo/Blueprints/Abilities/` | 例如 `GA_BasicAttack` |
| 项目战斗蓝图 | `Content/BoldersDemo/Blueprints/Combat/` | 再按需要细分，例如 `Turns` |
| UI 控件蓝图 `WBP_...` | `Content/BoldersDemo/Blueprints/UI/` | UI 专属非蓝图素材放 `Assets/UI/` |
| 自建角色模型、动画序列、材质实例与角色专属数据 | `Content/BoldersDemo/Assets/Characters/Hero/` | 可引用第三方资产包的内容 |
| 项目战斗或技能数据资产 | `Content/BoldersDemo/Assets/Combat/` | 蓝图类放 `Blueprints/Combat/`；数据资产实例放这里 |
| 输入配置资产 | `Content/BoldersDemo/Assets/Input/` | 如 Input Action、Input Mapping Context |
| 正式地图与团队共同使用的测试地图 | `Content/BoldersDemo/Assets/Maps/` | 名称说明用途；个人临时试验放 `Developers/<用户名>/` |
| 外部导入的第三方角色资源 | `Content/Provider_PackName/` | 按来源或资产包单独保留顶层目录及其内部结构 |
| 项目 C++ 类 | `Source/BoldersDemo/` 的 `Public` / `Private` | 不是放在 Content 下；按职责建立对应子目录 |

项目资产可以直接引用第三方资产，**不必为了分类复制整套素材**。项目专用的子蓝图、材质实例、数据或适配资产放自有目录；它们引用外部素材时，该依赖仍需保留。需要直接修改第三方原件时先评估升级影响，记录修改及来源；若许可和技术条件允许，优先考虑自有派生资产或复制需要修改的部分，不假设所有资产都能无损继承或独立复制。

## 3. 第三方内容的导入与更新

1. 先区分普通源文件（如 FBX、PNG、WAV）、现成 UE 资产包、UE 插件。记录来源、版本、许可、获取日期，以及本项目做过的必要修改，便于以后查证和更新。
2. 普通源文件若属于第三方素材，可在 UE 导入对话框中选择与项目目录并列的 `Content/Provider_PackName/`，检查结果及依赖；自制源文件导入项目自有的 `Assets/` 对应功能目录。`.fbx`、`.png` 等原始文件不等同于导入后的 `.uasset`，建议按重导入需要保存在项目外有备份的素材库并记录来源。
3. Fab/Marketplace 等现成资产包可能自动安装到作者指定位置。保留其顶层目录和内部结构，先检查安装说明和更新方式；不要假设安装器支持任意目标路径，也不要为了目录统一而随意搬动或改名。
4. 从其他 UE 项目迁移时使用编辑器的 **Migrate**，查看依赖报告，选择目标项目的 `Content` 目录；迁移通常保留源项目相对路径。迁移前检查同名路径、依赖及覆盖风险，必要时先在单独的中转项目检查。
5. 第三方 UE 插件通常位于项目的 `Plugins/` 或引擎插件目录，遵循插件自己的安装和升级说明；外部 C++ 库也不是内容浏览器资产，应按模块或插件的集成方式处理。
6. 第三方顶层目录只是组织方式，不会自动排除打包。升级、删除或清理前查看引用、项目打包配置和许可；目录位置本身不证明是否会进入最终包。

## 4. C++ 源码

- 延续现有 `Source/BoldersDemo` 单模块，按职责逐步增加目录，例如 `Public/Characters/` 对应 `Private/Characters/`，或 `Public/Combat/` 对应 `Private/Combat/`。只有模块内部使用的头文件可放 `Private`；需要被其他模块包含的头文件放 `Public`。`.cpp` 放 `Private`。
- Public/Private 表示模块边界，与 C++ 类内的 `public` / `private` 访问控制不同；蓝图能否使用由 UE 反射和相应声明决定，不由文件夹名称直接决定。
- 添加模块依赖时按实际使用维护 `BoldersDemo.Build.cs`；只有公共接口需要暴露的依赖才放 Public，否则优先 Private。移动现有类时检查 include 和构建；改动类名、模块名或代码后需考虑蓝图引用、反射和构建影响。
- 项目规模确有需要时再讨论新模块或插件；目前不预设 GAS、网络功能或其他未确定的架构。

## 5. 基本命名

自有文件夹使用简短英文名称，保持一致；UE 自有资产建议使用 `类型前缀_名称_可选描述`。第三方原包不强制批量改名。

| 类型 | 示例 |
| --- | --- |
| 蓝图类 | `BP_Hero` |
| 动画蓝图 | `ABP_Hero` |
| UI 控件蓝图 | `WBP_CombatHUD` |
| 地图 | `L_CombatTest` |
| 静态网格体 / 骨骼网格体 | `SM_Crate` / `SK_Hero` |
| 材质 / 材质实例 | `M_Stone` / `MI_Stone_Dark` |
| 纹理 | `T_Stone_BaseColor` / `T_Stone_Normal` |
| 数据资产 | `DA_Ability_Fireball` |

例子只表示命名方法，不表示项目已实现这些内容。C++ 类遵循 UE 的 `A`、`U`、`F` 等类型前缀惯例和现有模块风格。

## 6. 整理与迁移已有内容

- 迁移地图前先确认版本控制状态和编辑器状态，在内容浏览器移动，处理 Redirectors，检查并按需更新默认地图配置、字符串路径和其他引用，再检查启动地图及运行。编辑器迁移和修复不能假设会自动更新所有配置或硬编码路径。
- UE 资产在内容浏览器中移动、重命名；检查并修复 Redirectors、保存受影响资产，再核验引用。不要在资源管理器中直接搬 `.uasset` 或 `.umap`。C++ 文件移动另按代码、include、工程文件及编译检查处理。
- 个人试验目录中的内容不应被正式内容依赖；项目共同使用的测试资产放项目自有目录。清理之前先检查引用。

## 7. 参考资料

- [Epic：资产命名建议](https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects)
- [Epic：Unreal Engine Modules](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-modules)
- [Epic：Lyra 示例项目](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
- [Allar：UE 风格指南与内容目录](https://github.com/Allar/ue5-style-guide#2-content-directory-structure)
- [Epic：迁移资产](https://dev.epicgames.com/documentation/en-us/unreal-engine/migrating-assets-in-unreal-engine)；[资产 Redirectors](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-redirectors-in-unreal-engine)
