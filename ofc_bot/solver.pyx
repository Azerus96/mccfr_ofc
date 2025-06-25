# cython: language_level=3, language=c++
# distutils: language = c++
from libcpp.string cimport string

cdef extern from "mccfr_solver.hpp" namespace "ofc":
    cdef cppclass MCCFRSolver:
        MCCFRSolver()
        void train(int iterations)
        void save_strategy(const string& path)
        void load_strategy(const string& path)

cdef class Solver:
    cdef MCCFRSolver* solver_ptr

    def __cinit__(self):
        self.solver_ptr = new MCCFRSolver()

    def __dealloc__(self):
        del self.solver_ptr

    def train(self, int iterations):
        self.solver_ptr.train(iterations)

    def save(self, path):
        cdef string path_str = path.encode('UTF-8')
        self.solver_ptr.save_strategy(path_str)

    def load(self, path):
        cdef string path_str = path.encode('UTF-8')
        self.solver_ptr.load_strategy(path_str)
