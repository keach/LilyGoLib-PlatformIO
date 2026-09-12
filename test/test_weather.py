import os
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class WeatherLogicTest(unittest.TestCase):
    def test_mapping_aggregation_and_cache_age(self):
        compiler = shutil.which("c++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("A C++ compiler is required")
        source_text = textwrap.dedent(r"""
            #include <cassert>
            #include <cstring>
            #include "weather_logic.h"

            int main() {
                assert(std::strcmp(weatherConditionJapanese(200), "雷雨") == 0);
                assert(std::strcmp(weatherConditionJapanese(500), "雨") == 0);
                assert(std::strcmp(weatherConditionJapanese(701), "霧") == 0);
                assert(std::strcmp(weatherConditionJapanese(800), "晴れ") == 0);
                assert(std::strcmp(weatherConditionJapanese(801), "薄曇り") == 0);
                assert(std::strcmp(weatherConditionJapanese(804), "曇り") == 0);
                assert(std::strcmp(weatherConditionJapanese(999), "不明") == 0);

                // 2026-09-12 00:00 UTC, JST is 09:00.
                const time_t now = 1789171200;
                WeatherForecastPoint points[] = {
                    {now - 3600, 180, 200, 90, 500}, // past: ignored today
                    {now + 3600, 190, 230, 10, 801}, // 10:00 JST
                    {now + 3 * 3600, 170, 250, 70, 800}, // noon JST
                    {now + 15 * 3600, 160, 210, 20, 500}, // midnight next day
                    {now + 27 * 3600, 180, 260, 80, 802}, // noon next day
                };
                DailyWeather today, tomorrow;
                assert(aggregateWeatherForecast(points, 5, now, 9 * 3600,
                                                today, tomorrow));
                assert(today.valid && tomorrow.valid);
                assert(today.minimum_temperature_tenths == 170);
                assert(today.maximum_temperature_tenths == 250);
                assert(today.maximum_precipitation_percent == 70);
                assert(today.condition_id == 800);
                assert(tomorrow.minimum_temperature_tenths == 160);
                assert(tomorrow.maximum_temperature_tenths == 260);
                assert(tomorrow.maximum_precipitation_percent == 80);
                assert(tomorrow.condition_id == 802);

                WeatherSnapshot snapshot;
                assert(weatherRefreshDue(snapshot, now));
                assert(weatherCacheStale(snapshot, now));
                snapshot.valid = true;
                snapshot.updated_epoch = now;
                assert(!weatherRefreshDue(snapshot, now + 12 * 3600 - 1));
                assert(weatherRefreshDue(snapshot, now + 12 * 3600));
                assert(!weatherCacheStale(snapshot, now + 24 * 3600));
                assert(weatherCacheStale(snapshot, now + 24 * 3600 + 1));
                return 0;
            }
        """)
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "weather_test.cpp"
            executable = Path(temp_dir) / "weather_test"
            source.write_text(source_text, encoding="utf-8")
            subprocess.run([
                compiler, "-std=c++17", f"-I{ROOT / 'src'}", str(source),
                str(ROOT / "src" / "weather_logic.cpp"), "-o", str(executable)
            ], check=True, env=os.environ.copy())
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
