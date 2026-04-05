# cython: freethreading_compatible = True

from types import GenericAlias

cimport cython
from cpython.dict cimport (
    PyDict_Contains,
    PyDict_Copy,
    PyDict_SetItem
)
from cpython.set cimport PySet_Contains
from cpython.tuple cimport PyTuple_GET_SIZE

from . import abc
from . import utils

# uvloop strategy to prevent crashing.
cdef Reducable = abc.Reducable
cdef varnames = utils.varnames
del abc
del utils

cdef extern from "reduce_packer.h":
    # added incase of preformance degrades and to prevent cython
    # from playing around with or triggering segfaults.
    object reduce_call(
        object kwds, 
        object r_wrapped,
        object r_defaults, 
        object r_required,
        object r_params,
        const Py_ssize_t n_required,
        const Py_ssize_t n_params
    )
    # This one was a performance optimization 
    # in dictionary iterations
    int reduce_install_kwargs(
        object params,
        object kwargs,
        object output
    )

cdef extern from "Python.h":
    Py_ssize_t PyDict_GET_SIZE(dict p)


@cython.freelist(250)
cdef class reduce:
    cdef:
        public object __wrapped__
        dict _defaults
        str _name
        Py_ssize_t _nargs, _nparams
        tuple _optional
        tuple _params
        frozenset _param_set
        tuple _required

    __class_getitem__ = classmethod(GenericAlias)

    def __init__(
        self,
        object func
    ) -> None:
        cdef tuple required
        cdef dict optional
        # The only bottlekneck is here when ititalizing althought this part is not planned to be benchmarked.
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
        self._param_set = frozenset(self._params)
        self._nparams = len(self._params)
        self._required = required

    @cython.nonecheck(False)
    def install(self, *args, **kwargs):
        r"""Simillar to `inspect.BoundArguments` but a little bit faster,
        it is based off CPython's getargs.c's algorythms, this will also attempt to
        install defaults if any are needed. However this does not allow arbitrary
        arguments to be passed through. Instead, this should primarly be
        used for writing callback utilities that require a parent function's signature.

        :raises TypeError: if argument parsing fails or has a argument that overlaps in either args or kwargs.
        """
        # Mimics checks from vgetargskeywordsfast_impl in getargs.c
        cdef Py_ssize_t nargs = PyTuple_GET_SIZE(args)
        cdef Py_ssize_t ntotal = nargs +  PyDict_GET_SIZE(kwargs)
        cdef Py_ssize_t n
        cdef frozenset params = self._param_set
        cdef object k, v
        cdef dict output

        if ntotal < self._nargs:
            raise TypeError(f"Not enough params in {self._name}")

        elif ntotal > self._nparams:
            raise TypeError(
                "%.200s takes at most %d %sargument%s (%i given)" % (
                    self._name,
                    self._nparams,
                    "keyword" if not self._nargs else "",
                    "" if self._nparams == 1 else "s",
                    ntotal
                )
            )

        # Begin parsing while checking for overlapping arguments and copy off all the defaults.

        output = PyDict_Copy(self._defaults)
        for n in range(nargs):
            # FIXME: Currently Cython still None Checks
            k = self._params[n]
            if PyDict_Contains(kwargs, k):
                # arg present in tuple and dict
                raise TypeError(
                    "argument for %.200s given by name ('%s') and position (%d)" % (
                    self._name, k, n + 1
                    )
                )
            # XXX: Will let it take the heat for right now 
            # until we can figure out how to make it stop segfaulting.
            v = args[n]
            PyDict_SetItem(output, k, v)

        if reduce_install_kwargs(params, kwargs, output) < 0:
            raise 
        return output

    @cython.nonecheck(False)
    def __call__(self, dict kwds):
        """Calls reduction wrapper and calls function
        while ignoring any unwanted arguments. This is useful
        when chaining together callbacks with different function
        formations."""

        return reduce_call(
            kwds, 
            self.__wrapped__, 
            self._defaults,
            self._required, 
            self._params, 
            self._nargs,
            self._nparams
        )


Reducable.register(reduce)

