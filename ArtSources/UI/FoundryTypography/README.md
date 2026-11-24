# 火山铸造岛界面字图

九张 PNG 均使用内置 GPT Image 生成。原图像素与透明通道保持原样，`Atlas.json` 记录每条字图的 UV 及原图长宽比。Unreal 导入入口为 `Scripts/Editor/UI/import_foundry_typography.py`，资产位于 `/Game/BoldersDemo/Assets/UI/FoundryTypography`。选角界面隐藏旧字标，战斗 HUD 用原生字体实时绘制数值。

## 生成提示词

### Boss.png

```text
Generate a production-ready Chinese text sprite atlas for original top-down volcanic foundry action game. Use true transparent PNG backdrop. The first two lines are premium bold bronze-gold engraved Chinese game boss title lettering, with the second hotter as if inlaid with glowing amber. Remaining lines are clear condensed ivory with fine dark keyline. Exactly 1 equal-width column(s) by 9 equally spaced rows, text centered inside each cell and vertically aligned with the other cell in that row. Keep generous transparent gutters between rows and columns, and clear outside padding. Canvas portrait for a single column, square or slightly landscape for two columns. Nothing except the requested words. No panels, icons, background, captions, extra decoration, lines, numbering, or separator bars. Do not invent, translate, repeat, or omit text. Text enclosed by quotes below is exactly what must be written; quotes, line numbers, and bars are instructions and must not be drawn. In order from the top:
1. "熔炉骑士"
2. "狂暴熔炉骑士"
3. "苏醒"
4. "狂暴蓄势"
5. "失衡"
6. "重砸"
7. "旋扫"
8. "跳劈"
9. "收势" No interpuncts or middle dots anywhere. Draw a perfectly transparent negative space around the clean letter contours, with no halo or fill behind the text.
```

### Main.png

```text
Generate a production-ready Chinese text sprite atlas for original top-down volcanic foundry action game. Use true transparent PNG backdrop. Crisp warm-ivory simplified Chinese and Latin type, minimal antique forge influence, practical HUD type legible at small sizes. Exactly 2 equal-width column(s) by 11 equally spaced rows, text centered inside each cell and vertically aligned with the other cell in that row. Keep generous transparent gutters between rows and columns, and clear outside padding. Canvas portrait for a single column, square or slightly landscape for two columns. Nothing except the requested words. No panels, icons, background, captions, extra decoration, lines, numbering, or separator bars. Do not invent, translate, repeat, or omit text. Text enclosed by quotes below is exactly what must be written; quotes, line numbers, and bars are instructions and must not be drawn. In order from the top:
1. "火山铸造岛"   |   "铸造岛已夺回"
2. "全队倒下"   |   "战斗用时"
3. "再战 [R]"   |   "守卫"
4. "波"   |   "剩余"
5. "1P"   |   "2P"
6. "目标"   |   "Esc 收起"
7. "Esc / 点击收起"   |   "已倒下"
8. "等队友推进后复活"   |   "等待本局结算"
9. "选择职业"   |   "确认出战"
10. "清除选择"   |   "等待服务器"
11. "选角失败"   |   "已选择"
```

### Skills.png

```text
Generate a production-ready Chinese text sprite atlas for original top-down volcanic foundry action game. Use true transparent PNG backdrop. High contrast crisp ivory Simplified Chinese UI sprites with understated sturdy forge typography. Make '+' an actual plus button glyph and Latin keycaps just text, without surrounding boxes. Exactly 2 equal-width column(s) by 15 equally spaced rows, text centered inside each cell and vertically aligned with the other cell in that row. Keep generous transparent gutters between rows and columns, and clear outside padding. Canvas portrait for a single column, square or slightly landscape for two columns. Nothing except the requested words. No panels, icons, background, captions, extra decoration, lines, numbering, or separator bars. Do not invent, translate, repeat, or omit text. Text enclosed by quotes below is exactly what must be written; quotes, line numbers, and bars are instructions and must not be drawn. In order from the top:
1. "战士"   |   "弓手"
2. "法师"   |   "队员"
3. "剑盾"   |   "双手剑"
4. "蘸取"   |   "水刃"
5. "冰刃"   |   "雷刃"
6. "普通箭"   |   "爆裂箭"
7. "冰霜箭"   |   "弹射箭"
8. "火球"   |   "水浪"
9. "冰锥"   |   "落雷"
10. "灼热"   |   "寒意"
11. "潮湿"   |   "带电"
12. "冻结"   |   "F"
13. "E"   |   "Tab"
14. "WASD"   |   "Esc"
15. "+"
```

### Signs.png

```text
Generate a production-ready Chinese text sprite atlas for original top-down volcanic foundry action game. Use true transparent PNG backdrop. Use bold ivory headings for first four lines and plain highly legible medium-weight ivory interface type for remaining instructional Chinese lines. Punctuation verbatim. Exactly 1 equal-width column(s) by 13 equally spaced rows, text centered inside each cell and vertically aligned with the other cell in that row. Keep generous transparent gutters between rows and columns, and clear outside padding. Canvas portrait for a single column, square or slightly landscape for two columns. Nothing except the requested words. No panels, icons, background, captions, extra decoration, lines, numbering, or separator bars. Do not invent, translate, repeat, or omit text. Text enclosed by quotes below is exactly what must be written; quotes, line numbers, and bars are instructions and must not be drawn. In order from the top:
1. "铸造岛入口"
2. "旧铸道"
3. "核心石台"
4. "冷却槽"
5. "沿外缘下行，找到入口守卫。"
6. "WASD 移动，左键攻击"
7. "Tab 切换攻距，点击地图展开"
8. "沿旧铸道前往西侧下层。"
9. "岩卫重砸后，趁收势反击。"
10. "清掉西侧守卫，沿坡道上台。"
11. "石像会苏醒，先躲预警再反击。"
12. "冰冻水面，导柱通电。"
13. "战士靠近水槽按 F 附魔。" No interpuncts or middle dots anywhere. Draw a perfectly transparent negative space around the clean letter contours, with no halo or fill behind the text.
```

### Combat.png

```text
Generate a production-ready Chinese text sprite atlas for original top-down volcanic foundry action game. Use true transparent PNG backdrop. Compact medium-weight ivory Simplified Chinese HUD lettering with a thin black keyline, tiny screen legibility. Exactly 2 equal-width column(s) by 11 equally spaced rows, text centered inside each cell and vertically aligned with the other cell in that row. Keep generous transparent gutters between rows and columns, and clear outside padding. Canvas portrait for a single column, square or slightly landscape for two columns. Nothing except the requested words. No panels, icons, background, captions, extra decoration, lines, numbering, or separator bars. Do not invent, translate, repeat, or omit text. Text enclosed by quotes below is exactly what must be written; quotes, line numbers, and bars are instructions and must not be drawn. In order from the top:
1. "熔炉喷口"   |   "冷却槽"
2. "炸药桶"   |   "闪电导柱"
3. "余烬突袭者"   |   "孢弹投手"
4. "玄武岩卫"   |   "喷发中"
5. "压力上升"   |   "冷却中"
6. "已结冰"   |   "导电中"
7. "汽化中"   |   "可施加潮湿"
8. "未点燃"   |   "已充能"
9. "现身中"   |   "蓄力中"
10. "孢弹在空中"   |   "护壳已打开"
11. "收势中"   |   "已被打断"
```

### Feedback.png

```text
Generate a production-ready Chinese text sprite atlas for original top-down volcanic foundry action game. Use true transparent PNG backdrop. Very clean compact medium-weight ivory Simplified Chinese HUD copy with a dark keyline, readable at small size. Exactly 2 equal-width column(s) by 11 equally spaced rows, text centered inside each cell and vertically aligned with the other cell in that row. Keep generous transparent gutters between rows and columns, and clear outside padding. Canvas portrait for a single column, square or slightly landscape for two columns. Nothing except the requested words. No panels, icons, background, captions, extra decoration, lines, numbering, or separator bars. Do not invent, translate, repeat, or omit text. Text enclosed by quotes below is exactly what must be written; quotes, line numbers, and bars are instructions and must not be drawn. In order from the top:
1. "已预备出招"   |   "冷却中"
2. "已冻结"   |   "目标已失效"
3. "无法出招"   |   "超出攻距"
4. "攻击路线受阻"   |   "当前招式对该机关无效"
5. "接近目标"   |   "接近路线受阻"
6. "目标已离开攻距"   |   "机关状态已变化"
7. "需靠近可用的冷却槽"   |   "路线受阻"
8. "水或冰可压住喷发"   |   "可导电、结冰或汽化"
9. "受击后延时爆炸"   |   "受击后向相连冷却槽放电"
10. "突刺可侧闪"   |   "投掷可打断"
11. "重砸后护壳打开"
```

### Reject.png

```text
Generate a production-ready Chinese text sprite atlas for original top-down volcanic foundry action game. Use true transparent PNG backdrop. Clean compact medium ivory simplified Chinese HUD microcopy, subtle dark shadow. Exactly 1 equal-width column(s) by 7 equally spaced rows, text centered inside each cell and vertically aligned with the other cell in that row. Keep generous transparent gutters between rows and columns, and clear outside padding. Canvas portrait for a single column, square or slightly landscape for two columns. Nothing except the requested words. No panels, icons, background, captions, extra decoration, lines, numbering, or separator bars. Do not invent, translate, repeat, or omit text. Text enclosed by quotes below is exactly what must be written; quotes, line numbers, and bars are instructions and must not be drawn. In order from the top:
1. "喷口只能用水或冰压制"
2. "等水槽散去蒸汽"
3. "冰封水槽可用火融开"
4. "导电水槽可用冰或火改变状态"
5. "冷却槽响应冰、火或雷"
6. "炸药桶已点燃"
7. "导柱正在重新充能"
```

### Selection.png

```text
Create exact original RPG interface art on true transparent RGBA. Professional dark volcanic foundry fantasy select-screen graphics. Main two title treatments use antique bronze and ivory, buttons 确认职业 and 取消选择 are wide symmetrical beveled plaques with proportional lettering and solid dark bronze or slate soles, 已选择 gets a teal outlined little badge, and 1P and 2P are small dark medallion badges in teal and amber respectively. The wait/failure and three role captions are free-standing warm ivory texts. Layout 2 equal columns by 6 equal rows in a strict grid, each isolated by a wide transparent gutter and completely contained, no overlap, no duplicate, no art beyond specified cells. Row-major exact text:
"BoldersDemo" | "职业选择"
"确认职业" | "取消选择"
"正在进入…" | "进入失败，请重试"
"✓ 已选择" | "1P"
"2P" | "战士"
"弓手" | "法师". Cited quotation marks, separators or extra words are not part of the graphics.
```

### Arrows.png

```text
Production asset for a Simplified Chinese in-game tiny skill label. EXACTLY two rows on genuinely TRANSPARENT RGBA, both horizontally centered, with lots of completely transparent space between them. Top row says exactly "爆炸箭". Bottom row says exactly "冰冻箭". Large bold warm ivory sans-serif Chinese strokes, thin near-black outline only, crisp small-size readability. Flat typographic text. Absolutely NO UI plate, frame, badge, geometric shapes, texture fill between letters, glow, panel, illustrations, numbers, or any other words. Treat the text as two isolated typographic glyph groups.
```