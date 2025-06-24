from libcpp.string cimport string

cdef extern from "mccfr_solver.hpp" namespace "ofc":
    cdef cppclass MCCFRSolver:
        MCCFRSolver()
        void train(int iterations)
        void save_strategy(const string& path)
        void load_strategy(const string& path)
