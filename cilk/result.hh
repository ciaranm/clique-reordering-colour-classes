/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef CODE_GUARD_RESULT_HH
#define CODE_GUARD_RESULT_HH 1

#include <set>
#include <list>
#include <chrono>

struct Result
{
    /// The clique
    std::set<int> clique;

    /// Total number of nodes processed.
    unsigned long long nodes = 0;

    /**
     * Runtimes. The first entry in the list is the total runtime.
     * Additional values are for each worker thread.
     */
    std::list<std::chrono::milliseconds> times;
};

#endif
