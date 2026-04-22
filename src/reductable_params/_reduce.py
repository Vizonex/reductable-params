import os
import sys

NO_EXTENSIONS = bool(os.environ.get("REDUCTABLE_PARAMS_NO_EXTENSIONS"))  # type: bool
if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True

# isort: off
if not NO_EXTENSIONS:  # pragma: no branch
    try:
        from ._reduce_c import reduce as reduce_c

        reduce = reduce_c  # type: ignore[misc, assignment]
    except ImportError:  # pragma: no cover
        from ._reduce_py import reduce as reduce_py

        reduce = reduce_py  # type: ignore[misc, assignment]
else:
    from ._reduce_py import reduce as reduce_py

    reduce = reduce_py  # type: ignore[misc, assignment]
# isort: on
