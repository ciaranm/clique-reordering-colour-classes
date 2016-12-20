/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef CODE_GUARD_CLIQUE_HH
#define CODE_GUARD_CLIQUE_HH 1

#include "params.hh"
#include "result.hh"

#include <vector>
#include <set>

struct Graph
{
    unsigned size = 0;
    std::vector<std::set<unsigned> > edges;
};

auto clique(const Graph & graph, const Params & params) -> Result;

#endif
