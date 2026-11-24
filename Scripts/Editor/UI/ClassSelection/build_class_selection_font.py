"""Subset the SIL OFL Noto Serif SC display face for the class-selection UI."""
import argparse
import re
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

ROOT = Path(__file__).resolve().parents[4]
OUTPUT = ROOT / "ArtSources/UI/ClassSelection/Style"

def fonts(font_path):
    # Font families are renamed because these are small, static subsets for the game's UI.
    glyphs = "".join(chr(i) for i in range(32, 127)) + "…"
    for file in (
        "Scripts/Editor/UI/ClassSelection/setup_class_selection.py",
        "Source/BoldersDemo/Private/Editor/UI/ClassSelectionSetup.cpp",
        "Source/BoldersDemo/Private/UI/ClassSelection/ClassSelectionWidget.cpp",
        "Source/BoldersDemo/Private/UI/ClassSelection/ClassCardWidget.cpp",
        "Source/BoldersDemo/Private/Player/BoldersPlayerControllerClassSelection.cpp",
    ):
        text = (ROOT / file).read_text(encoding="utf-8-sig")
        glyphs += "".join(re.findall(r"[\u3000-\u303f\u3400-\u9fff\uff01-\uff60]", text))
    for style, weight in (("Display",900),("Text",650)):
        font = instantiateVariableFont(TTFont(font_path), {"wght": weight})
        options = subset.Options()
        options.name_IDs = ["*"]
        options.name_languages = ["*"]
        sub = subset.Subsetter(options=options)
        sub.populate(text=glyphs)
        sub.subset(font)
        names = font["name"]
        family = f"Bolders Selection {style}"
        replacements = {1: family, 2: "Regular", 3: family, 4: family, 6: family.replace(" ",""),
                        16: family, 17:"Regular"}
        for record in names.names:
            if record.nameID in replacements:
                record.string = replacements[record.nameID]
        font.save(OUTPUT / f"BoldersSelection{style}.ttf")



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/NotoSerifSC-VF.ttf"))
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    fonts(args.font)
