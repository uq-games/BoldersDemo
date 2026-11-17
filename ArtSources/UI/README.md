# HealthBarFrame

`HealthBarFrame.png` 使用内置 GPT Image / imagegen 生成，原始尺寸 2172×724，RGBA。原图完整保留，不通过裁剪或重绘替代生成结果。Unreal 导入为 `/Game/BoldersDemo/Assets/UI/Health/T_HealthBarFrame`，HUD 用 UV 排除透明留白，血量填充与数字在运行时绘制。源图随项目保存，供重新导入。

生成提示词：

> Create a production-ready 2D game UI sprite: ONE single horizontal empty health-bar frame, centered, very wide about 6:1 aspect ratio within a tightly fitted canvas. Stylized fantasy adventure matching cute low-poly knights and mushroom monsters. Elegant warm aged brass border, dark charcoal inner empty well, subtle bevels, small symmetrical diamond end caps. Straight rectangular inner well, clear long continuous space for a dynamically drawn health fill. No text, no numbers, no icons, no filled health, no scene, no drop shadow outside, no extra frames. Orthographic flat front view. Transparent background outside the frame. This will be reused as both the top-left party health bar frame and a small enemy overhead health bar.

生成参数：`transparent_background=true`。未使用 API/CLI fallback。
