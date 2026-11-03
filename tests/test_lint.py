# This might be a little safter than needing to
# use pre-commit which is a known diskspace problem but this
# has it's own weaknesses. It's inspiration comes from the way
# uvloop & winloop both handle the use of linting.
# For now this will primarly be used with ruff
# but mypy is coming soon.

import os
import subprocess
import typing

import pytest


class RuffLinter:
    CMD: list[str]

    def __init_subclass__(cls, *, cmd: list[str]):
        cls.CMD = cmd
        super().__init_subclass__()

    def find_cwd(self):
        return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    def get_bin(self) -> str:
        if not typing.TYPE_CHECKING:
            ruff = pytest.importorskip(
                "ruff", reason="No ruff linter avalible"
            )
        else:  # pragma: no branch
            import ruff  # type: ignore[import-untyped]
        return ruff.find_ruff_bin()

    def run_cmd(self) -> None:
        try:
            subprocess.run(
                [self.get_bin(), *self.CMD],
                check=True,
                stderr=subprocess.PIPE,
                stdout=subprocess.PIPE,
                cwd=self.find_cwd(),
            )
        except subprocess.CalledProcessError as ex:
            output = ex.stdout.decode()
            output += "\n"
            output += ex.stderr.decode()
            raise AssertionError(
                "ruff validation failed: {}\n{}".format(ex, output)
            ) from None

    def test_source_code(self) -> None:
        self.run_cmd()


class TestFormatting(RuffLinter, cmd=["format", "--check", "--diff"]):
    pass


class TestSouceCode(RuffLinter, cmd=["check"]):
    pass


class TestMypy:
    def test_mypy(self) -> None:
        if not typing.TYPE_CHECKING:
            api = pytest.importorskip("mypy.api")
        else:
            from mypy import api
        _, stderr, status = api.run(["tests", "src"])
        assert status == 0, stderr
