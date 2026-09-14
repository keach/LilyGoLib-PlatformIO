import os
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class GotifyLogicTest(unittest.TestCase):
    def test_filter_order_and_automatic_interval(self):
        compiler = shutil.which("c++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("A C++ compiler is required")
        source_text = textwrap.dedent(r"""
            #include <cassert>
            #include "gotify_logic.h"

            int main() {
                GotifyMessage messages[] = {
                    {12, 5, "third", "", ""},
                    {10, 5, "old", "", ""},
                    {11, 5, "second", "", ""},
                };
                const size_t count = filterNewGotifyMessages(messages, 3, 10);
                assert(count == 2);
                assert(messages[0].id == 11);
                assert(messages[1].id == 12);

                assert(gotifyAutomaticCheckDue(100, 0, false));
                assert(!gotifyAutomaticCheckDue(100 + 5 * 60 * 1000 - 1,
                                                 100, true));
                assert(gotifyAutomaticCheckDue(100 + 5 * 60 * 1000,
                                                100, true));
                // Unsigned subtraction keeps the interval valid across
                // millis() wraparound.
                assert(gotifyAutomaticCheckDue(
                    20, 0xFFFFFFFFU - 5 * 60 * 1000, true));
                return 0;
            }
        """)
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "gotify_test.cpp"
            executable = Path(temp_dir) / "gotify_test"
            source.write_text(source_text, encoding="utf-8")
            subprocess.run([
                compiler, "-std=c++17", f"-I{ROOT / 'src'}", str(source),
                str(ROOT / "src" / "gotify_logic.cpp"), "-o", str(executable)
            ], check=True, env=os.environ.copy())
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
