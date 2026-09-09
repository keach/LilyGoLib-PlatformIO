import os
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ClockDateFormatterTest(unittest.TestCase):
    def test_japanese_date_format_and_weekdays(self):
        compiler = shutil.which("c++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("A C++ compiler is required for the formatter test")

        test_source = textwrap.dedent(
            r'''
            #include <cassert>
            #include <cstdio>
            #include <cstring>
            #include "clock_date_formatter.h"

            int main() {
                const char *weekdays[] = {
                    "日", "月", "火", "水", "木", "金", "土"
                };
                for (int weekday = 0; weekday < 7; ++weekday) {
                    struct tm value = {};
                    value.tm_year = 2026 - 1900;
                    value.tm_mon = 9 - 1;
                    value.tm_mday = 10;
                    value.tm_wday = weekday;

                    char actual[32];
                    formatJapaneseClockDate(actual, sizeof(actual), value);

                    char expected[32];
                    snprintf(expected, sizeof(expected),
                             "2026年9月10日 (%s)", weekdays[weekday]);
                    assert(strcmp(actual, expected) == 0);
                }

                struct tm single_digits = {};
                single_digits.tm_year = 2026 - 1900;
                single_digits.tm_mon = 1 - 1;
                single_digits.tm_mday = 2;
                single_digits.tm_wday = 5;
                char actual[32];
                formatJapaneseClockDate(actual, sizeof(actual), single_digits);
                assert(strcmp(actual, "2026年1月2日 (金)") == 0);
                return 0;
            }
            '''
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "clock_date_formatter_test.cpp"
            executable = Path(temp_dir) / "clock_date_formatter_test"
            source.write_text(test_source, encoding="utf-8")
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(source),
                    str(ROOT / "src" / "clock_date_formatter.cpp"),
                    "-o",
                    str(executable),
                ],
                check=True,
                env=os.environ.copy(),
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
