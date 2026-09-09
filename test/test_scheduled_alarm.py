import os
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ScheduledAlarmTest(unittest.TestCase):
    def test_next_trigger_and_single_shot_behavior(self):
        compiler = shutil.which("c++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("A C++ compiler is required for the model test")

        test_source = textwrap.dedent(
            r"""
            #include <cassert>
            #include <cstdlib>
            #include <ctime>
            #include "scheduled_alarm.h"

            time_t epoch(int year, int month, int day,
                         int hour, int minute, int second) {
                struct tm value = {};
                value.tm_year = year - 1900;
                value.tm_mon = month - 1;
                value.tm_mday = day;
                value.tm_hour = hour;
                value.tm_min = minute;
                value.tm_sec = second;
                value.tm_isdst = -1;
                return mktime(&value);
            }

            int main() {
                setenv("TZ", "JST-9", 1);
                tzset();

                const time_t morning = epoch(2026, 9, 1, 6, 30, 0);
                assert(nextScheduledAlarmEpoch(morning, 7, 0) ==
                       epoch(2026, 9, 1, 7, 0, 0));
                const time_t evening = epoch(2026, 9, 1, 23, 30, 0);
                assert(nextScheduledAlarmEpoch(evening, 7, 0) ==
                       epoch(2026, 9, 2, 7, 0, 0));
                const time_t exact = epoch(2026, 9, 1, 7, 0, 0);
                assert(nextScheduledAlarmEpoch(exact, 7, 0) ==
                       epoch(2026, 9, 2, 7, 0, 0));

                ScheduledAlarm alarm;
                assert(alarm.configure(morning, 7, 0, true));
                assert(alarm.enabled());
                assert(alarm.secondsUntilTrigger(morning) == 30 * 60);
                alarm.update(epoch(2026, 9, 1, 6, 59, 59));
                assert(!alarm.alerting());
                alarm.update(epoch(2026, 9, 1, 7, 0, 0));
                assert(alarm.alerting());
                assert(!alarm.enabled());
                assert(alarm.triggerEpoch() == 0);
                alarm.stopAlert();
                assert(!alarm.alerting());

                assert(alarm.configure(evening, 0, 5, false));
                assert(!alarm.enabled());
                assert(alarm.setEnabled(true, evening));
                assert(alarm.triggerEpoch() ==
                       epoch(2026, 9, 2, 0, 5, 0));
                assert(alarm.setEnabled(false, evening));
                assert(!alarm.enabled());

                ScheduledAlarm adjusted;
                assert(adjusted.configure(morning, 7, 0, true));
                assert(adjusted.triggerEpoch() ==
                       epoch(2026, 9, 1, 7, 0, 0));
                const time_t clock_moved_back =
                    epoch(2026, 8, 31, 6, 30, 0);
                assert(adjusted.reschedule(clock_moved_back));
                assert(adjusted.triggerEpoch() ==
                       epoch(2026, 8, 31, 7, 0, 0));
                const time_t clock_moved_forward =
                    epoch(2026, 9, 2, 8, 0, 0);
                assert(adjusted.reschedule(clock_moved_forward));
                assert(adjusted.triggerEpoch() ==
                       epoch(2026, 9, 3, 7, 0, 0));
                assert(adjusted.enabled());
                assert(!adjusted.alerting());

                ScheduledAlarm restored;
                assert(restored.restore(7, 0, true,
                                        epoch(2026, 9, 2, 7, 0, 0)));
                restored.update(epoch(2026, 9, 3, 8, 0, 0));
                assert(restored.alerting());
                assert(!restored.enabled());
                assert(!restored.restore(24, 0, false, 0));
                return 0;
            }
            """
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "scheduled_alarm_test.cpp"
            executable = Path(temp_dir) / "scheduled_alarm_test"
            source.write_text(test_source, encoding="utf-8")
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(source),
                    str(ROOT / "src" / "scheduled_alarm.cpp"),
                    "-o",
                    str(executable),
                ],
                check=True,
                env={**os.environ, "TZ": "JST-9"},
            )
            subprocess.run(
                [str(executable)],
                check=True,
                env={**os.environ, "TZ": "JST-9"},
            )

    def test_runtime_wake_and_notification_output(self):
        compiler = shutil.which("c++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("A C++ compiler is required for the runtime test")

        test_source = textwrap.dedent(
            r"""
            #include <cassert>
            #include <cstdlib>
            #include <ctime>
            #include "scheduled_alarm_runtime.h"

            int wake_count = 0;
            NotificationOutputState last_output;

            void wake(void *) { ++wake_count; }
            void output(NotificationOutputState state, void *) {
                last_output = state;
            }

            time_t epoch(int hour, int minute, int second) {
                struct tm value = {};
                value.tm_year = 2026 - 1900;
                value.tm_mon = 8;
                value.tm_mday = 1;
                value.tm_hour = hour;
                value.tm_min = minute;
                value.tm_sec = second;
                value.tm_isdst = -1;
                return mktime(&value);
            }

            int main() {
                setenv("TZ", "JST-9", 1);
                tzset();
                ScheduledAlarm alarm;
                const time_t now = epoch(6, 59, 0);
                assert(alarm.configure(now, 7, 0, true));
                ScheduledAlarmRuntime runtime(alarm);
                runtime.setWakeCallback(wake);
                runtime.setAlertOutputCallback(output);
                runtime.setNotificationMode(NotificationMode::SoundOnly);
                runtime.setNotificationSoundPreset(
                    NotificationSoundPreset::Urgent);

                uint32_t delay = 0;
                assert(runtime.nextWakeDelaySeconds(now, delay));
                assert(delay == 60);
                runtime.update(epoch(6, 59, 59), 900);
                assert(wake_count == 0);
                runtime.update(epoch(7, 0, 0), 1000);
                assert(wake_count == 1);
                assert(alarm.alerting());
                assert(!alarm.enabled());
                assert(runtime.requiresAwake());
                assert(last_output.target == NotificationTarget::ScheduledAlarm);
                assert(last_output.sound_preset ==
                       NotificationSoundPreset::Urgent);
                assert(last_output.sound_active);
                assert(!last_output.vibration_active);

                alarm.stopAlert();
                runtime.update(epoch(7, 0, 1), 2000);
                assert(!runtime.requiresAwake());
                assert(!last_output.sound_active);
                assert(!last_output.vibration_active);

                // Simulate a Light Sleep wake where the ESP32 clock has
                // reached 07:00:03 while the external RTC still reads
                // 06:59:57. The runtime must be evaluated with the refreshed
                // RTC time and wait for the wall-clock deadline.
                ScheduledAlarm drifted_alarm;
                assert(drifted_alarm.configure(epoch(6, 30, 0),
                                               7, 0, true));
                ScheduledAlarmRuntime drifted_runtime(drifted_alarm);
                const time_t stale_system_time = epoch(7, 0, 3);
                const time_t refreshed_rtc_time = epoch(6, 59, 57);
                assert(stale_system_time > drifted_alarm.triggerEpoch());
                drifted_runtime.update(refreshed_rtc_time, 3000);
                assert(!drifted_alarm.alerting());
                assert(drifted_alarm.secondsUntilTrigger(
                           refreshed_rtc_time) == 3);
                drifted_runtime.update(epoch(7, 0, 0), 6000);
                assert(drifted_alarm.alerting());
                return 0;
            }
            """
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "scheduled_alarm_runtime_test.cpp"
            executable = Path(temp_dir) / "scheduled_alarm_runtime_test"
            source.write_text(test_source, encoding="utf-8")
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(source),
                    str(ROOT / "src" / "scheduled_alarm.cpp"),
                    str(ROOT / "src" / "scheduled_alarm_runtime.cpp"),
                    str(ROOT / "src" / "end_notification.cpp"),
                    str(ROOT / "src" / "notification_sound.cpp"),
                    "-o",
                    str(executable),
                ],
                check=True,
                env={**os.environ, "TZ": "JST-9"},
            )
            subprocess.run(
                [str(executable)], check=True,
                env={**os.environ, "TZ": "JST-9"},
            )


if __name__ == "__main__":
    unittest.main()
