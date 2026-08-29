import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PomodoroTimerTest(unittest.TestCase):
    def test_focus_break_cycle_and_wraparound(self):
        compiler = shutil.which("c++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("A C++ compiler is required for the model test")

        test_source = textwrap.dedent(
            r"""
            #include <cassert>
            #include <cstdint>
            #include "pomodoro_timer.h"
            #include "pomodoro_timer_runtime.h"

            struct CallbackState {
                int wakes = 0;
                NotificationOutputState output;
            };

            void wake(void *context) {
                static_cast<CallbackState *>(context)->wakes++;
            }

            void output(NotificationOutputState value, void *context) {
                static_cast<CallbackState *>(context)->output = value;
            }

            int main() {
                PomodoroTimer timer;
                assert(timer.phase() == PomodoroPhase::Focus);
                assert(timer.state() == PomodoroState::Idle);
                assert(timer.remainingSeconds(0) == 25 * 60);
                assert(timer.start(1000));
                assert(timer.remainingSeconds(1000) == 25 * 60);
                assert(timer.pause(2000));
                assert(timer.state() == PomodoroState::Paused);
                assert(timer.remainingSeconds(9000) == 25 * 60 - 1);
                assert(timer.resume(9000));
                timer.update(9000 + (25 * 60 - 1) * 1000);
                assert(timer.state() == PomodoroState::Alerting);
                assert(timer.completedFocusSessions() == 1);

                assert(timer.startNextPhase(2000000));
                assert(timer.phase() == PomodoroPhase::Break);
                assert(timer.remainingSeconds(2000000) == 5 * 60);
                timer.update(2000000 + 5 * 60 * 1000);
                assert(timer.state() == PomodoroState::Alerting);
                assert(timer.completedFocusSessions() == 1);
                assert(timer.startNextPhase(3000000));
                assert(timer.phase() == PomodoroPhase::Focus);

                timer.reset();
                assert(timer.phase() == PomodoroPhase::Focus);
                assert(timer.state() == PomodoroState::Idle);
                assert(timer.completedFocusSessions() == 0);

                PomodoroTimer wrapping;
                const uint32_t near_wrap = UINT32_MAX - 500;
                assert(wrapping.start(near_wrap));
                wrapping.update(near_wrap +
                                PomodoroTimer::kFocusDurationMs - 1);
                assert(wrapping.state() == PomodoroState::Running);
                wrapping.update(near_wrap +
                                PomodoroTimer::kFocusDurationMs);
                assert(wrapping.state() == PomodoroState::Alerting);

                PomodoroTimer notified;
                PomodoroTimerRuntime runtime(notified);
                CallbackState callbacks;
                runtime.setWakeCallback(wake, &callbacks);
                runtime.setAlertOutputCallback(output, &callbacks);
                runtime.setNotificationMode(NotificationMode::SoundOnly);
                runtime.setNotificationSoundPreset(
                    NotificationSoundPreset::Urgent);
                assert(notified.start(0));
                runtime.update(0);
                uint32_t remaining_ms = 0;
                assert(runtime.nextWakeDelayMilliseconds(0, remaining_ms));
                assert(remaining_ms == PomodoroTimer::kFocusDurationMs);
                runtime.update(PomodoroTimer::kFocusDurationMs);
                assert(callbacks.wakes == 1);
                assert(callbacks.output.target ==
                       NotificationTarget::PomodoroTimer);
                assert(callbacks.output.sound_preset ==
                       NotificationSoundPreset::Urgent);
                assert(callbacks.output.sound_active);
                assert(!callbacks.output.vibration_active);
                assert(runtime.requiresAwake());
                assert(notified.startNextPhase(
                    PomodoroTimer::kFocusDurationMs));
                runtime.update(PomodoroTimer::kFocusDurationMs);
                assert(!callbacks.output.sound_active);
                return 0;
            }
            """
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "pomodoro_timer_test.cpp"
            executable = Path(temp_dir) / "pomodoro_timer_test"
            source.write_text(test_source, encoding="utf-8")
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(source),
                    str(ROOT / "src" / "pomodoro_timer.cpp"),
                    str(ROOT / "src" / "pomodoro_timer_runtime.cpp"),
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
