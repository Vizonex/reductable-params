import inspect
from dataclasses import dataclass
from statistics import median
from timeit import repeat

import matplotlib.pyplot as plt

# from pluggy import PluginManager
from reductable_params._reduce import reduce as reduce_c
from reductable_params._reduce_py import reduce as reduce_py


@dataclass
class result:
    using: str
    slowest: float
    median: float
    fastest: float

    def data(self):
        return {
            f"{self.using}-fastest": self.fastest,
            f"{self.using}-average": self.median,
            f"{self.using}-slowest": self.slowest,
        }


def draw_graph(results: list[result], save_as: str, title: str, x: str):
    plt.rcParams.update({"figure.autolayout": True})
    plt.style.use("fivethirtyeight")
    fig, ax = plt.subplots()

    everything = {}
    for r in results:
        everything.update(r.data())

    group_names = everything.keys()
    group_data = everything.values()

    ax.barh(group_names, group_data)
    labels = ax.get_xticklabels()
    ax.set(xlabel=x, title=title, xlim=[0, 0.2])
    plt.setp(labels, horizontalalignment="right")
    fig.savefig(save_as)


def benchmark_installing():
    data: list[tuple[int, ...]] = [
        (69352, 24186, 93538),
        (42261, 0x10),
        (97576, 66765, 164341),
        (60793, 61199, 121992),
        (68644, 51343, 119987),
        (35468, 76592, 112060),
        (16869, 15525, 32394),
        (62388, 25419, 87807),
        (38822, 16992),
        (19364, 29353, 48717),
    ]

    def test(a, b, c=None):
        pass

    rd_c = reduce_c(test)
    rd_py = reduce_py(test)
    sig = inspect.signature(test)

    def test_rd_c():
        for d in data:
            args = rd_c.install(*d)
            assert isinstance(args["a"], int)
            assert args["a"] == d[0]

    def test_rd_py():
        for d in data:
            args = rd_py.install(*d)
            assert isinstance(args["a"], int)
            assert args["a"] == d[0]

    def test_signature():
        for d in data:
            args = sig.bind(*d)
            args.apply_defaults()
            assert isinstance(args.arguments["a"], int)
            assert args.arguments["a"] == d[0]

    def bench_func(func, name: str):
        # warm up so that results are accurate.
        for _ in range(30):
            func()
        times = list(
            map(lambda x: x * 100, repeat(func, repeat=1000, number=10))
        )

        return result(name, max(times), median(times), min(times))

    results = [
        bench_func(test_signature, "inspect standard library"),
        bench_func(test_rd_py, "reduce / pure python"),
        bench_func(test_rd_c, "reduce / pure C"),
    ]
    draw_graph(
        results, "install-benchmark.png", "Packing parameters", "100000 calls"
    )
    return results


if __name__ == "__main__":
    benchmark_installing()
