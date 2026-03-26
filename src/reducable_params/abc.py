import sys
from abc import ABC, abstractmethod
from collections.abc import Callable
from typing import Any, Generic, TypeVar

if sys.version_info < (3, 10):
    from typing_extensions import ParamSpec
else:
    from typing import ParamSpec

if sys.version_info < (3, 13):
    from typing_extensions import TypeIs
else:
    from typing import TypeIs

T = TypeVar("T")
P = ParamSpec("P")

# Mostly added for pytest's sake but also to prevent breaking
# if cython is utilized with reducable params but a python version
# is exposed sort of scenario.


class Reducable(Generic[P, T], ABC):
    __wrapped__: Callable[P, T]

    @abstractmethod
    def __init__(self, func: Callable[P, T]) -> None: ...
    @abstractmethod
    def install(self, *args: P.args, **kwargs: P.kwargs) -> dict[str, Any]: ...
    @abstractmethod
    def __call__(self, /, kwds: dict[str, Any]) -> T: ...


def is_reducable(obj: object) -> TypeIs[Reducable]:
    """Used for inspecting to see if a type belongs to a `reduce`
    class-like object"""
    return isinstance(obj, Reducable)
