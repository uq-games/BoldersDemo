# 职业选择带字素材

`Controls.png`、`Labels.png`、`PlainNormal.png` 和 `PlainSelected.png` 均由内置 GPT Image 生成或编辑。这里保存原始图像，Unreal 通过 UV 取用对应区域。

- `Controls.png` 包含完整带字的按钮、徽牌和标识。
- `PlayerSlots.png` 是 GPT Image 生成的 1P 和 2P 徽牌图集，沿用 `Controls.png` 的切角底板样式。
- `Labels.png` 提供标题、进入中和失败提示。
- `ClassCaptions.png` 是 GPT Image 生成的双语职业名图集。中文沿用暖白宋体风格，英文以较小的衬线大写呈现在下方。两行在同一张贴图上居中对齐。
- `PlainNormal.png` 和 `PlainSelected.png` 是去掉定位铭牌与分隔线的卡片底图。

在 UE 中运行 `Scripts/Editor/update_class_selection_style.py` 可重导入并重新应用。替换图集后需要核对 `ClassSelectionSetup.cpp` 的取图范围与脚本里的职业名称取图范围。

## Labels

Use case: ui-mockup, production transparent text sprite atlas for the existing fantasy game opening screen screenshot reference. ONLY typography on truly TRANSPARENT background, no plaques, decoration, illustration, gradients behind text, checkerboard, or additional writing. Widescreen 3 columns by 2 rows with six separate centrally aligned tiles, generous completely empty gutters between and around. Use one consistent traditional Chinese Song/Ming serif font with bright warm ivory fill and very restrained dark shadow; simplified Chinese glyphs must all be real, correct and crisply legible. Every tile uses perfectly centered text. Top-left cell: large title "职业选择" (exactly 4 characters). Top-middle tile: "战士" on large line then "近身挥剑" on small line. Top-right tile: "弓箭手" on large line then "拉弓射击" on small line. Bottom-left cell: "法师" on large line then "施放魔法弹" on small line. Each of those 3 title+description tiles has EXACT same type sizes, weight and vertical gap, one name and one description only. Bottom-middle cell: "正在进入…" in one medium-size line. Bottom-right cell: "进入失败，请重试" in one medium-size line. Write all exact characters. Centered rows with no overlap or visible cell borders. Quiet high-end painterly RPG typographic treatment like reference, very crisp at 1080p. Entire atlas has invisible background, use clear alpha. Reference only for typography mood and ivory tone.

## Controls

Use case ui-mockup; production-ready fully TRANSPARENT sprite atlas with 6 independent small UI components in exact readable Chinese. Existing BoldersDemo appearance from refs: warm ivory Song serif (Chinese), authentic warm antique gold bevels, dark slate, translucent rich teal only on active badge. Layout exactly THREE columns across by TWO rows, 6 individual sprites on the same 16:9 otherwise fully transparent canvas, one sprite centered in each equal-size cell, generous separation, no boundaries or instructions printed. Top left: icon of three small rocks and the gold serif text “BoldersDemo” as a single wordmark without plaque, rocks left. Top middle: large, wide golden bevel button like reference, EXACT “确认职业” in centered bold dark brown Chinese Song; center entire text block horizontally and vertically within the gold face, small symmetrical diamond ends. Top right: a wide dark blue-gray antique brass beveled button marked “取消选择”, exact wording, perfectly vertically centered; NO arrow/icon, text centered both axes. Bottom left: dark blue-gray small gold rimmed capsule with center text “本地玩家” (four exact characters) only, no pictogram. Bottom middle: compact dark teal badge outlined in mint cyan, an ivory check mark followed by “已选择”; center the check + text grouping horizontally and vertically as a unit, match appearance in screenshot. Bottom right: completely empty. Text and surfaces are crisp, refine actual optical centering, each on a single plate with integrated label. 2D orthographic front-facing styling. Only letters listed, no watermarks, no scene, no collage panels. Genuine alpha everywhere outside sprite silhouettes.

## PlainNormal

Use case precise-object-edit. Edit the single existing BoldersDemo dark slate and antique-gold portrait card skin. Keep exact geometry, size, corners, textured metal, surface, composition and existing transparent margin. Remove only the lower role/title hexagonal plate and the nearby gold line/diamond. Paint in those parts seamlessly with the same matte dark slate texture as adjoining card face, keeping lower quarter flat and clean. Result is a completely empty opaque slate card with its original gold frame and alpha outside. No text, no badge, no other content. Preserve aspect and pixel positioning as closely as possible.

## PlainSelected

Use case precise-object-edit. Edit the single existing selected BoldersDemo card skin. Keep the original exact shape, size, gold diagonal corners, brilliant cyan-mint illuminated thin beveled edges and bloom, dark teal texture, exact orientation, and transparent exterior. Remove the small outlined upper-right check plaque including the checkmark. Remove the small lower gold role plaque and gold line/diamond. Fill all three removed elements with the same clean dark teal card face texture as surroundings. Empty selected card with transparent exterior and existing bright cyan edges. No typography, no badge, no other content. Preserve original outer positions and portrait canvas aspect.
