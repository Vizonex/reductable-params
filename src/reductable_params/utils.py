import inspect
import sys
from types import CodeType
from typing import Any

_PYPY = hasattr(sys, "pypy_version_info")

# Forked from pluggy with my own modification.
# This is also a PR/idea based off this that I have for pluggy to try.
# SEE: https://github.com/pytest-dev/pluggy/pull/659

_POSITONAL_ONLY = inspect.Parameter.POSITIONAL_ONLY
_POSITONAL_OR_KW = inspect.Parameter.POSITIONAL_OR_KEYWORD


def _varnames_from_code(
    func: object,
) -> tuple[tuple[str, ...], dict[str, Any]]:
    """Faster shortcut than needing to parse a function's given signature."""
    code: CodeType = getattr(func, "__code__")
    args = code.co_varnames[: code.co_argcount]

    if defaults := getattr(func, "__defaults__", None):
        index = -len(defaults)
        return args[:index], dict(zip(args[index:], defaults))
    else:
        return args, {}


def _varnames_from_signature(
    func: object,
) -> tuple[tuple[str, ...], dict[str, Any]]:
    """extracts from a function's given signature but is slightly slower"""
    sig = inspect.signature(func)  # type: ignore[arg-type]
    parameters = sig.parameters
    required_args = [
        k for k, v in parameters.items() if v.kind == _POSITONAL_ONLY
    ]
    optional = {
        k: v for k, v in parameters.items() if v.kind == _POSITONAL_OR_KW
    }
    return tuple(required_args), optional


def varnames(func: object):
    """Return tuple of positional and keywrord argument names along with
    defaults for a function, method, class or callable.

    In case of a class, its ``__init__`` method is considered.
    For methods the ``self`` parameter is not included.

    """
    if inspect.isclass(func):
        try:
            func = func.__init__
        except AttributeError:  # pragma: no cover - pypy special case
            return (), {}
    elif not inspect.isroutine(func):  # callable object?
        try:
            func = getattr(func, "__call__", func)
        except Exception:  # pragma: no cover - pypy special case
            return (), {}

    try:
        # func MUST be a function or method here or we won't parse any args.
        func = func.__func__ if inspect.ismethod(func) else func
        if hasattr(func, "__code__") and inspect.isroutine(func):
            # Take the optimized approch rather than sit and parse the given
            # signature.
            args, kwargs = _varnames_from_code(
                func
                if not hasattr(func, "__wrapped__")
                else inspect.unwrap(func)
            )
        else:
            # Fallback
            args, kwargs = _varnames_from_signature(func)
    except TypeError:  # pragma: no cover
        return (), {}

    # strip any implicit instance arg
    # pypy3 uses "obj" instead of "self" for default dunder methods
    if not _PYPY:
        implicit_names: tuple[str, ...] = ("self",)
    else:  # pragma: no cover
        implicit_names = ("self", "obj")
    if args:
        qualname: str = getattr(func, "__qualname__", "")
        if inspect.ismethod(func) or (
            "." in qualname and args[0] in implicit_names
        ):
            args = args[1:]

    return args, kwargs
