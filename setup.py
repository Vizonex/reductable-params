from setuptools import Extension, setup

if __name__ == "__main__":
    setup(
        ext_modules=[
            Extension(
                "reductable_params._reduce",
                ["src/reductable_params/_reduce.c"],
            )
        ]
    )
