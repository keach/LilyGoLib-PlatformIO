import subprocess
import unittest

from support.firmware_version import (
    collect_firmware_metadata,
    cpp_string_literal,
)


class FakeGitRunner:
    def __init__(self, responses):
        self.responses = responses

    def __call__(self, arguments):
        key = tuple(arguments)
        response = self.responses.get(key)
        if isinstance(response, Exception):
            raise response
        if response is None:
            raise subprocess.CalledProcessError(1, ["git", *arguments])
        return response


class FirmwareVersionTest(unittest.TestCase):
    def base_responses(self):
        return {
            ("rev-parse", "--short=7", "HEAD"): "a1b2c3d",
            ("show", "-s", "--format=%ct", "HEAD"): "0",
            ("status", "--porcelain", "--untracked-files=normal"): "",
            ("describe", "--tags", "--abbrev=0"): "v0.1",
        }

    def test_tagged_clean_version_and_jst_commit_date(self):
        metadata = collect_firmware_metadata(
            ".", FakeGitRunner(self.base_responses())
        )

        self.assertEqual(metadata.version, "v0.1+a1b2c3d")
        self.assertEqual(metadata.commit_date, "1970-01-01 09:00 JST")
        self.assertEqual(metadata.commit_day, "1970-01-01")
        self.assertEqual(metadata.commit_time, "09:00")
        self.assertTrue(metadata.commit_date_known)
        self.assertFalse(metadata.dirty)

    def test_uses_untagged_and_dirty_suffix(self):
        responses = self.base_responses()
        responses[("describe", "--tags", "--abbrev=0")] = (
            subprocess.CalledProcessError(128, ["git", "describe"])
        )
        responses[("status", "--porcelain", "--untracked-files=normal")] = (
            " M src/main.cpp"
        )

        metadata = collect_firmware_metadata(".", FakeGitRunner(responses))

        self.assertEqual(metadata.version, "untagged+a1b2c3d-dirty")
        self.assertTrue(metadata.dirty)

    def test_git_failure_uses_stable_fallback(self):
        metadata = collect_firmware_metadata(".", FakeGitRunner({}))

        self.assertEqual(metadata.version, "unknown+unknown")
        self.assertEqual(metadata.commit_date, "unknown")
        self.assertFalse(metadata.commit_date_known)

    def test_same_repository_state_is_deterministic(self):
        responses = self.base_responses()

        first = collect_firmware_metadata(".", FakeGitRunner(responses))
        second = collect_firmware_metadata(".", FakeGitRunner(responses))

        self.assertEqual(first, second)

    def test_cpp_string_literal_escapes_quotes_and_backslashes(self):
        self.assertEqual(
            cpp_string_literal('tag"with\\characters'),
            '\\"tag\\"with\\\\characters\\"',
        )


if __name__ == "__main__":
    unittest.main()
