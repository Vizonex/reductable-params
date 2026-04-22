from __future__ import annotations

import pytest

from reductable_params._reduce_c import reduce as reduce_c
from reductable_params._reduce_py import reduce as reduce_py
from reductable_params.abc import Reducable, is_reducable


@pytest.fixture(
    params=[
        (85022, 43727, 128749),
        (14444, 84793, 99237),
        (54194, 7697, 61891),
        (89370, 55869, 145239),
        (81581, 85501, 167082),
        (64190, 90931, 155121),
        (24752, 78572, 103324),
        (31516, 8151, 39667),
        (67001, 96812, 163813),
        (31638, 95508, 127146),
    ]
)
def addition_cases(request: pytest.FixtureRequest) -> tuple[int, ...]:
    return request.param


class BaseTestReduce:
    reduce: type[Reducable]

    def make_test_1(self) -> Reducable[[str, int | None], None]:
        def sig(a: str, b: int | None = None):
            pass

        return self.reduce(sig)

    def test_properties_1(self):
        func = self.make_test_1()
        assert func.args == ("a",)
        assert func.kwargs == ("b",)

    def test_properties_are_readonly(self):
        func = self.make_test_1()
        with pytest.raises(
            AttributeError, match=r"args property is read-only."
        ):
            func.args = ("override",)

        with pytest.raises(
            AttributeError, match=r"kwargs property is read-only."
        ):
            func.kwargs = ("override",)

    def test_install(self) -> None:
        func = self.make_test_1()
        assert {"a": "1", "b": 2} == func.install(a="1", b=2)  # type: ignore[call-arg]
        assert {"a": "1", "b": 2} == func.install("1", b=2)  # type: ignore[call-arg]
        assert {"a": "1", "b": 2} == func.install("1", 2)
        assert {"a": "1", "b": None} == func.install(a="1")  # type: ignore[call-arg]
        assert {"a": "1", "b": None} == func.install("1")  # type: ignore[call-arg]

    def test_bad_install_too_many_arguments(self):
        func = self.make_test_1()
        with pytest.raises(TypeError):
            # Too Many Arguments
            func.install("1", 2, 3)

    def test_bad_install_too_little_arguments(self):
        func = self.make_test_1()
        with pytest.raises(BaseException):
            # Too Little Arguments
            func.install()

    def test_bad_install_overlapping(self):
        func = self.make_test_1()
        with pytest.raises(TypeError):
            # Overlapping tuple and dict keywords
            func.install("1", a="10")

    def test_calling_returnables(self, addition_cases: tuple[int, ...]):
        def addition_cb(a: int, b: int):
            return a + b

        a, b, c = addition_cases
        func = self.reduce(addition_cb)
        assert func({"a": a, "b": b}) == c

    def test_call_raises(self):
        class Problem(Exception):
            pass

        def raise_me():
            raise Problem("problem...")

        func = self.reduce(raise_me)

        with pytest.raises(Problem, match=r"problem..."):
            func({})

    def test_call_with_unwanted_arguments(self):
        def i_require_foo(foo: str):
            assert foo == "SPAM"

        func = self.reduce(i_require_foo)
        # It's what reduce's job is intended to do.
        # Allow creating pluggy-like systems but at a
        # very low level...
        func({"foo": "SPAM", "extra": "BLAH BLAH BLAH"})

    def test_is_reducable_check(self):
        assert is_reducable(self.make_test_1())
        assert is_reducable(0xDEAD) is False


class TestCReduce(BaseTestReduce):
    reduce = reduce_c  # type: ignore[assignment]


class TestPyReduce(BaseTestReduce):
    reduce = reduce_py  # type: ignore[assignment]
