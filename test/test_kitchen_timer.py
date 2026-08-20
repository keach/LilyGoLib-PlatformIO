import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class KitchenTimerTest(unittest.TestCase):
    def test_timer_and_shared_notification_model(self):
        compiler = shutil.which("c++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("A C++ compiler is required for the model test")

        test_source = textwrap.dedent(
            r"""
            #include <cassert>
            #include "end_notification.h"
            #include "kitchen_timer.h"

            int main() {
                KitchenTimer timer;
                assert(timer.configuredSeconds() == 0);
                assert(!timer.start(100));

                timer.adjustSeconds(-10);
                assert(timer.configuredSeconds() == 0);
                timer.adjustSeconds(10);
                assert(timer.configuredSeconds() == 10);
                assert(timer.start(100));
                assert(timer.remainingSeconds(100) == 10);
                assert(timer.remainingSeconds(101) == 10);
                assert(timer.pause(1100));
                assert(timer.remainingSeconds(9000) == 9);
                timer.reset();
                assert(timer.state() == KitchenTimerState::Idle);
                assert(timer.configuredSeconds() == 0);

                assert(timer.setDurationSeconds(10));
                assert(timer.start(100));
                assert(timer.pause(1100));
                assert(timer.resume(9000));
                timer.update(18000);
                assert(timer.state() == KitchenTimerState::Alerting);
                timer.stopAlert();
                assert(timer.state() == KitchenTimerState::Idle);

                assert(timer.setDurationSeconds(10000));
                assert(timer.configuredSeconds() == 99 * 60 + 59);
                timer.reset();
                assert(timer.configuredSeconds() == 0);

                EndNotification notification;
                notification.start(NotificationTarget::KitchenTimer,
                                   NotificationMode::SoundAndVibration,
                                   NotificationSoundPreset::DoubleBeep, 1000);
                auto output = notification.output(1000);
                assert(output.sound_active && output.vibration_active);
                assert(output.sound_preset ==
                       NotificationSoundPreset::DoubleBeep);
                output = notification.output(2000);
                assert(!output.sound_active && !output.vibration_active);

                notification.setMode(NotificationMode::SoundOnly);
                output = notification.output(3000);
                assert(output.sound_active && !output.vibration_active);
                notification.setMode(NotificationMode::VibrationOnly);
                output = notification.output(3000);
                assert(!output.sound_active && output.vibration_active);

                notification.update(30999);
                assert(notification.active());
                notification.update(31000);
                assert(!notification.active());
                output = notification.output(31000);
                assert(!output.sound_active && !output.vibration_active);

                assert(notificationSoundPresetCount() == 3);
                assert(resolveNotificationSoundPreset(255) ==
                       kDefaultNotificationSoundPreset);
                assert(nextNotificationSoundPreset(
                           NotificationSoundPreset::Classic) ==
                       NotificationSoundPreset::DoubleBeep);
                assert(previousNotificationSoundPreset(
                           NotificationSoundPreset::Classic) ==
                       NotificationSoundPreset::Ascending);
                assert(notificationSoundFrequencyAt(
                           NotificationSoundPreset::Classic, 0) == 1000);
                assert(notificationSoundFrequencyAt(
                           NotificationSoundPreset::DoubleBeep, 200) == 0);
                assert(notificationSoundFrequencyAt(
                           NotificationSoundPreset::DoubleBeep, 350) == 880);
                assert(notificationSoundFrequencyAt(
                           NotificationSoundPreset::Ascending, 100) == 659);
                assert(notificationSoundFrequencyAt(
                           NotificationSoundPreset::Ascending, 300) == 784);
                assert(notificationSoundFrequencyAt(
                           NotificationSoundPreset::Ascending, 600) == 1047);
                assert(notificationSoundFrequencyAt(
                           NotificationSoundPreset::Ascending, 900) == 0);
                return 0;
            }
            """
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "kitchen_timer_test.cpp"
            executable = Path(temp_dir) / "kitchen_timer_test"
            source.write_text(test_source, encoding="utf-8")
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(source),
                    str(ROOT / "src" / "kitchen_timer.cpp"),
                    str(ROOT / "src" / "end_notification.cpp"),
                    str(ROOT / "src" / "notification_sound.cpp"),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
