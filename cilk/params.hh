/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef CODE_GUARD_PARAMS_HH
#define CODE_GUARD_PARAMS_HH 1

#include <chrono>
#include <atomic>

struct Params
{
    /// If this is set to true, we should abort due to a time limit.
    std::atomic<bool> * abort;

    /// The start time of the algorithm.
    std::chrono::time_point<std::chrono::steady_clock> start_time;

    /// How much sorting to do?
    enum { no_sorting, defer1, full_sort } how_much_sorting = no_sorting;

    /// Prime the incumbent?
    unsigned prime = 0;

    /// Decision problem instead?
    unsigned decide = 0;

    /// Use parallel for instead of spawn?
    bool parallel_for = false;
};

#endif
