import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RANGES = ROOT / "support" / "character_sets" / "japanese_unicode_ranges.txt"
JIS_LEVEL1 = ROOT / "support" / "character_sets" / "jis_x_0208_level1.txt"
SAMPLES = (
    "2026年9月10日 (木)",
    "今日の天気は晴れ、降水確率は20％です。",
    "午前10時から定例会議",
    "新宿駅で待ち合わせ",
    "晴れ  最高28℃  最低20℃",
)


def requested_characters():
    characters = []
    for raw_line in RANGES.read_text(encoding="utf-8").splitlines():
        value = raw_line.split("#", 1)[0].strip()
        if not value:
            continue
        start_text, *end_text = value.split("-")
        start = int(start_text, 16)
        end = int(end_text[0], 16) if end_text else start
        characters.extend(chr(code_point) for code_point in range(start, end + 1))
    characters.extend("".join(JIS_LEVEL1.read_text(encoding="utf-8").split()))
    return tuple(dict.fromkeys(characters))


class JapaneseFontTest(unittest.TestCase):
    def test_jis_level1_has_expected_unique_character_count(self):
        characters = "".join(JIS_LEVEL1.read_text(encoding="utf-8").split())
        self.assertEqual(len(characters), 2965)
        self.assertEqual(len(set(characters)), 2965)

    def test_requested_set_contains_representative_ui_text(self):
        characters = set(requested_characters())
        self.assertEqual(len(characters), 3420)
        for sample in SAMPLES:
            with self.subTest(sample=sample):
                self.assertTrue(set(sample) <= characters)

    def test_generated_fonts_cover_all_assigned_requested_characters(self):
        requested = set(requested_characters())
        unassigned_hiragana = {"\u3040", "\u3097", "\u3098"}
        for bpp in (2,):
            path = ROOT / "src" / "fonts" / f"lv_font_japanese_16_{bpp}bpp.c"
            source = path.read_text(encoding="utf-8")
            present = {
                chr(int(value, 16))
                for value in re.findall(r"U\+([0-9A-Fa-f]{4,6})", source)
            }
            with self.subTest(bpp=bpp):
                self.assertEqual(requested - present, unassigned_hiragana)
                self.assertIn(f"Bpp: {bpp}", source)
                self.assertIn(f"lv_font_japanese_16_{bpp}bpp", source)


if __name__ == "__main__":
    unittest.main()
