from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
import subprocess


JST = timezone(timedelta(hours=9))


@dataclass(frozen=True)
class FirmwareMetadata:
    tag: str
    commit: str
    dirty: bool
    commit_date: str

    @property
    def version(self):
        suffix = "-dirty" if self.dirty else ""
        return f"{self.tag}+{self.commit}{suffix}"

    @property
    def commit_day(self):
        return self.commit_date.split(" ", 1)[0]

    @property
    def commit_time(self):
        parts = self.commit_date.split(" ")
        return parts[1] if len(parts) >= 2 else "unknown"

    @property
    def commit_date_known(self):
        return self.commit_date != "unknown"


def run_git(project_dir, arguments):
    result = subprocess.run(
        ["git", "-C", project_dir, *arguments],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def collect_firmware_metadata(project_dir, git_runner=None):
    runner = git_runner or (lambda arguments: run_git(project_dir, arguments))

    try:
        commit = runner(["rev-parse", "--short=7", "HEAD"])
        commit_epoch = int(runner(["show", "-s", "--format=%ct", "HEAD"]))
        dirty = bool(
            runner(["status", "--porcelain", "--untracked-files=normal"])
        )
    except (OSError, subprocess.SubprocessError, TypeError, ValueError):
        return FirmwareMetadata(
            tag="unknown",
            commit="unknown",
            dirty=False,
            commit_date="unknown",
        )

    try:
        tag = runner(["describe", "--tags", "--abbrev=0"])
    except (OSError, subprocess.SubprocessError):
        tag = "untagged"

    commit_date = datetime.fromtimestamp(commit_epoch, JST).strftime(
        "%Y-%m-%d %H:%M JST"
    )
    return FirmwareMetadata(
        tag=tag or "untagged",
        commit=commit or "unknown",
        dirty=dirty,
        commit_date=commit_date,
    )


def cpp_string_literal(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'\\"{escaped}\\"'
