# 火山铸造岛路牌

四张透明 PNG 均由内置 GPT Image 生成，分别用于入口、旧铸道、核心石台和冷却槽。原图的箭头统一向右，游戏将图片的横轴对齐路牌在关卡中的朝向。

导入脚本位于 `Scripts/Editor/Maps/import_foundry_way_sign_art.py`，Unreal 资产位于 `/Game/BoldersDemo/Assets/Environment/VolcanoCombat/WaySigns`。该脚本需要启用渲染的编辑器，以便编译材质。

## 生成提示词

### Entry.png

```text
Use case: stylized-concept. In-game UV texture for a wayfinding plaque in an original low-poly volcanic foundry island. A broad basalt plaque reinforced with sand-cast copper corners, open gate pictogram engraved at the tail end, amber-orange arrow. Create only a single wide 3:2 plaque viewed perfectly straight from above, flat orthographic camera perpendicular to the horizontal face. Plaque near edge-to-edge, centered, clean compact silhouette and no pedestal. A single dominant oversized thick, solid, bright navigation arrow points precisely RIGHT (east, straight horizontal) with a wide triangular arrowhead and blunt rectangular shaft. Make the arrow roughly 75 percent of plaque width and 52 percent of plaque height with broad distinct shapes that read at 55 pixels. High contrast on a charcoal basalt base, a modest warm glow fully contained inside the opaque plaque, chunky stylized forged shapes, minimal surface noise. Edge worn volcanic-metal game art, no scene, no view of sides, no environment, no backing rectangle or cast shadow, no text, no additional arrows. Transparent background around plaque, genuine alpha.
```

### CastingRoad.png

```text
Use case: stylized-concept. In-game UV texture for a wayfinding plaque in an original low-poly volcanic foundry island. Long slab of heat-stained forged iron with scalloped copper edges and four large round rivets, small hammer-and-anvil pictogram engraved at the tail end, molten orange-gold arrow. Create only a single wide 3:2 plaque viewed perfectly straight from above, flat orthographic camera perpendicular to the horizontal face. Plaque near edge-to-edge, centered, clean compact silhouette and no pedestal. A single dominant oversized thick, solid, bright navigation arrow points precisely RIGHT (east, straight horizontal) with a wide triangular arrowhead and blunt rectangular shaft. Make the arrow roughly 75 percent of plaque width and 52 percent of plaque height with broad distinct shapes that read at 55 pixels. High contrast on a charcoal basalt base, a modest warm glow fully contained inside the opaque plaque, chunky stylized forged shapes, minimal surface noise. Edge worn volcanic-metal game art, no scene, no view of sides, no environment, no backing rectangle or cast shadow, no text, no additional arrows. Transparent background around plaque, genuine alpha.
```

### Core.png

```text
Use case: stylized-concept. In-game UV texture for a wayfinding plaque in an original low-poly volcanic foundry island. Weighty black basalt and cast bronze plaque with chiseled diagonal corners and subtle radial ribs, little furnace-core pictogram engraved at the tail end, vivid gold-yellow arrow. Create only a single wide 3:2 plaque viewed perfectly straight from above, flat orthographic camera perpendicular to the horizontal face. Plaque near edge-to-edge, centered, clean compact silhouette and no pedestal. A single dominant oversized thick, solid, bright navigation arrow points precisely RIGHT (east, straight horizontal) with a wide triangular arrowhead and blunt rectangular shaft. Make the arrow roughly 75 percent of plaque width and 52 percent of plaque height with broad distinct shapes that read at 55 pixels. High contrast on a charcoal basalt base, a modest warm glow fully contained inside the opaque plaque, chunky stylized forged shapes, minimal surface noise. Edge worn volcanic-metal game art, no scene, no view of sides, no environment, no backing rectangle or cast shadow, no text, no additional arrows. Transparent background around plaque, genuine alpha.
```

### Cooling.png

```text
Use case: stylized-concept. In-game UV texture for a wayfinding plaque in an original low-poly volcanic foundry island. Blackened cast iron and weathered copper plaque, small turquoise-patina grooves along the edge and a small water-drop pictogram engraved at the tail end, high-contrast warm amber arrow. Create only a single wide 3:2 plaque viewed perfectly straight from above, flat orthographic camera perpendicular to the horizontal face. Plaque near edge-to-edge, centered, clean compact silhouette and no pedestal. A single dominant oversized thick, solid, bright navigation arrow points precisely RIGHT (east, straight horizontal) with a wide triangular arrowhead and blunt rectangular shaft. Make the arrow roughly 75 percent of plaque width and 52 percent of plaque height with broad distinct shapes that read at 55 pixels. High contrast on a charcoal basalt base, a modest warm glow fully contained inside the opaque plaque, chunky stylized forged shapes, minimal surface noise. Edge worn volcanic-metal game art, no scene, no view of sides, no environment, no backing rectangle or cast shadow, no text, no additional arrows. Transparent background around plaque, genuine alpha.
```
