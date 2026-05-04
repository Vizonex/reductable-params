# cython: freethreading_compatible = True

from types import GenericAlias

cimport cython
from cpython.tuple cimport PyTuple_GET_SIZE

from . import abc
from . import utils

# uvloop strategy to prevent crashing.
cdef Reducable = abc.Reducable
cdef varnames = utils.varnames
del abc
del utils

# Macro-like object for controlling the heap-size 
# of the number of reduce elements to use.
# Throw an issue number should be bigger.
DEF REDUCE_FREELIST_SIZE = 250

cdef extern from "reduce_packer.h":
    # added incase of preformance degrades and to prevent cython
    # from playing around with or triggering segfaults.
    object reduce_call(
        object kwds, 
        object r_wrapped,
        object r_defaults, 
        object r_args,
        object r_params,
        const Py_ssize_t n_args,
        const Py_ssize_t n_params
    )
    # This one was a performance optimization 
    # in dictionary iterations
    int reduce_install_kwargs(
        object params,
        object kwargs,
        object output
    )
    # these typechecks will be whittled down to just object
    # later. This is here as a security check until
    # we can assume it's stable enough.
    object reduce_install_args(
        object name, # Function's possible name
        object defaults,
        object args, # tuple[Any, ...]
        object kwargs, # dict[str, Any]
        Py_ssize_t nargs, # PyTuple_GET_SIZE(args)
        object params  # tuple[Any, ...]
    )


cdef extern from "Python.h":
    Py_ssize_t PyDict_GET_SIZE(object p)


# TODO: ReduceObject structure in Header file and just link it here.
# to further migrate to Pure-C.
@cython.freelist(REDUCE_FREELIST_SIZE)
cdef class reduce:
    cdef:
        public object __wrapped__
        dict _defaults
        str _name
        Py_ssize_t _nargs, _nparams
        tuple _kwargs
        tuple _params
        frozenset _param_set
        tuple _args

    __class_getitem__ = classmethod(GenericAlias)

    def __init__(
        self,
        object func
    ) -> None:
        cdef tuple args
        cdef dict kwargs
        # The only bottlekneck is here when ititalizing althought this part is not planned to be benchmarked.
        args, kwargs = varnames(func)

        if name := getattr(func, "__name__", None):
            self._name = f"{name}()"
        else:
            self._name = "function"

        self.__wrapped__ = func
        self._defaults = kwargs
        self._nargs = len(args)
        self._kwargs = tuple(kwargs.keys())
        self._params = args + self._kwargs
        self._param_set = frozenset(self._params)
        self._nparams = len(self._params)
        self._args = args
    
    @property
    def args(self):
        """lists out the required arguments of this wrapped function."""
        return self._args
    
    @args.setter
    def args(self, value):
        raise AttributeError("args property is read-only.")

    @property
    def kwargs(self):
        """lists out optional arguments of this wrapped function."""
        return self._kwargs

    @kwargs.setter
    def kwargs(self, value):
        raise AttributeError("kwargs property is read-only.")


    @cython.nonecheck(False)
    def install(self, *args, **kwargs):
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
        cdef Py_ssize_t nargs = PyTuple_GET_SIZE(args)
        cdef Py_ssize_t ntotal = nargs +  PyDict_GET_SIZE(kwargs)
        cdef object output

        # TODO: This section needs to go into C Next...

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
        # NOTE: Cython still wants to make extra unwanted refs here
        # but when the full migration into pure-c is done this will 
        # ultimately be fixed.
        output = reduce_install_args(self._name, self._defaults, args, kwargs, nargs, self._params)
        if reduce_install_kwargs(self._param_set, kwargs, output) < 0:
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
            self._args, 
            self._params, 
            self._nargs,
            self._nparams
        )


Reducable.register(reduce)

