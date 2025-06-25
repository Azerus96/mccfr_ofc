from setuptools import setup, Extension
from Cython.Build import cythonize
import numpy
import glob

# Находим все C++ исходники, которые нужно скомпилировать
ompeval_sources = glob.glob("cpp_src/ompeval/omp/*.cpp")

# Наши собственные исходники
ofc_sources = [
    "cpp_src/game_state.cpp"
]

# Наша Cython-обертка
cython_ext = Extension(
    "ofc_bot.solver",
    sources=["ofc_bot/solver.pyx"] + ompeval_sources + ofc_sources,
    include_dirs=[
        numpy.get_include(),
        "cpp_src",
        "cpp_src/ompeval"
    ],
    language="c++",
    extra_compile_args=["-std=c++17", "-O3", "-fopenmp", "-march=native"],
    extra_link_args=["-fopenmp"]
)

setup(
    name="ofc_bot",
    version="1.1.0", # Финальная рабочая версия
    author="AI Solver",
    description="A compact, high-performance MCCFR solver for Pineapple OFC poker.",
    ext_modules=cythonize([cython_ext]),
    zip_safe=False,
    packages=['ofc_bot']
)
