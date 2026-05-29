import sys

from setuptools import Extension, setup

FLAGS = ["/O2" if sys.platform == "win32" else "-O3"]

if __name__ == "__main__":
    setup(
        ext_modules=[
            Extension(
                "reductable_params._reduce",
                ["src/reductable_params/_reduce.c"],
                extra_compile_args=FLAGS
            )
        ]
    )
