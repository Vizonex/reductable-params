from collections.abc import Callable
from typing import Any, Generic, NoReturn

from .abc import P, Reducable, T
from .utils import varnames


class reduce(Generic[P, T]):
    r"""reduceses arbitrary arguments being sent by only selecting
    ones that make sense on sending. Useful when chaining together
    callbacks where function's children may not need all arguments
    incase callback signatures differ from the parent."""

    __slots__ = (
        "__wrapped__",
        "_defaults",
        "_name",
        "_nargs",
        "_nparams",
        "_optional",
        "_params",
        "_params_set",
        "_required",
    )

    def __init__(
        self,
        func: Callable[P, T],
    ) -> None:
        # if for some reason inspect wants to grab it, let it do so...
        required, optional = varnames(func)

        if name := getattr(func, "__name__", None):
            self._name = f"{name}()"
        else:
            self._name = "function"

        self.__wrapped__ = func
        self._defaults = optional
        self._nargs = len(required)
        self._optional = tuple(optional.keys())
        self._params = required + self._optional
        self._nparams = len(self._params)
        self._params_set = frozenset(self._params)
        self._required = required

    @property
    def args(self) -> tuple[str, ...]:
        """lists out the required arguments of this wrapped function."""
        return self._required

    @args.setter
    def args(self, value: tuple[str, ...]) -> NoReturn:
        raise AttributeError("args property is read-only.")

    @property
    def kwargs(self) -> tuple[str, ...]:
        """lists out optional arguments of this wrapped function."""
        return self._optional

    @kwargs.setter
    def kwargs(self, value: tuple[str, ...]) -> NoReturn:
        raise AttributeError("kwargs property is read-only.")

    def install(self, *args: P.args, **kwargs: P.kwargs) -> dict[str, Any]:
        r"""Simillar to `inspect.BoundArguments` but a little bit faster,
        it is based off CPython's getargs.c's algorythms, this will also
        attempt to install defaults if any are needed. However this does not
        allow arbitrary arguments to be passed through. Instead, this should
        primarly be used for writing callback utilities that require a parent
        function's signature.

        :raises TypeError: if argument parsing fails or has a argument that
            overlaps in either args or kwargs.
        """

        # Mimics checks from vgetargskeywordsfast_impl in getargs.c
        ntotal = len(args) + len(kwargs)
        if ntotal < self._nargs:
            raise TypeError(f"Not enough params in {self._name}")

        elif ntotal > self._nparams:
            raise TypeError(
                "%.200s takes at most %d %sargument%s (%i given)"
                % (
                    self._name,
                    self._nparams,
                    "keyword" if not self._nargs else "",
                    "" if self._nparams == 1 else "s",
                    ntotal,
                )
            )

        # Begin parsing while checking for overlapping arguments and copy off
        # all the defaults.

        output = self._defaults.copy()
        for n, v in enumerate(args):
            k = self._params[n]
            if k in kwargs:
                # arg present in tuple and dict
                raise TypeError(
                    "argument for %.200s given by name ('%s') and position "
                    "(%d)" % (self._name, k, n + 1)
                )
            output[k] = v

        # replace rest of the defaults with keyword arguments
        for k, v in kwargs.items():
            # force up a keyerror if object is somehow
            # not present in the actual defaults
            if k not in self._params_set:
                raise KeyError(k)
            output[k] = v
        return output

    def __call__(self, kwds: dict[str, Any]) -> T:
        """Calls reduction wrapper and calls function
        while ignoring any unwanted arguments. This is useful
        when chaining together callbacks with different function
        formations."""

        kwargs = self._defaults.copy()
        args = [kwds[key] for key in self._required]

        for k in self._params[self._nargs :]:
            if k in kwargs:
                kwargs[k] = kwds[k]

        return self.__wrapped__(*args, **kwargs)


Reducable.register(reduce)
