import typing

import pytest

from reductable_params import reduce

if typing.TYPE_CHECKING:
    import msgspec
else:
    msgspec = pytest.importorskip(
        "msgspec", reason="msgspec is needed for this test"
    )


class Config(msgspec.Struct):
    x: int
    y: int = 20


def test_config():
    rd = reduce(Config)
    data = rd.install(x=1, y=10)
    result = rd(data)
    assert isinstance(result, Config)
