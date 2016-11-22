/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include "clique.hh"
#include "bit_graph.hh"
#include "template_voodoo.hh"

#include <algorithm>
#include <numeric>
#include <limits>
#include <iostream>
#include <atomic>
#include <mutex>

namespace
{
    struct Incumbent
    {
        std::atomic<unsigned> value{ 0 };

        std::mutex mutex;
        std::vector<unsigned> c;

        void update(const std::vector<unsigned> & new_c)
        {
            while (true) {
                unsigned current_value = value;
                if (new_c.size() > current_value) {
                    unsigned new_c_size = new_c.size();
                    if (value.compare_exchange_strong(current_value, new_c_size)) {
                        std::unique_lock<std::mutex> lock(mutex);
                        c = new_c;
                        std::cout << "-- " << new_c.size() << std::endl;
                        break;
                    }
                }
                else
                    break;
            }
        }
    };

    template <unsigned n_words_>
    struct Clique
    {
        const Params & params;

        FixedBitGraph<n_words_> graph;
        std::vector<int> order, invorder;
        Incumbent incumbent;

        std::atomic<unsigned long long> nodes;

        Clique(const Graph & g, const Params & q) :
            params(q),
            order(g.size),
            invorder(g.size),
            nodes(0)
        {
            // populate our order with every vertex initially
            std::iota(order.begin(), order.end(), 0);

            // pre-calculate degrees
            std::vector<int> degrees;
            degrees.resize(g.size);
            for (unsigned i = 0 ; i < g.size ; ++i)
                degrees[i] = g.edges[i].size();

            // sort on degree
            std::sort(order.begin(), order.end(),
                    [&] (int a, int b) { return true ^ (degrees[a] < degrees[b] || (degrees[a] == degrees[b] && a > b)); });

            // re-encode graph as a bit graph
            graph.resize(g.size);

            for (unsigned i = 0 ; i < order.size() ; ++i)
                invorder[order[i]] = i;

            for (unsigned i = 0 ; i < order.size() ; ++i)
                for (auto & e : g.edges[i])
                    graph.add_edge(invorder[i], invorder[e]);
        }

        auto colour_class_order(
                const FixedBitSet<n_words_> & p,
                std::array<unsigned, n_words_ * bits_per_word> & p_order,
                std::array<unsigned, n_words_ * bits_per_word> & p_bounds) -> void
        {
            FixedBitSet<n_words_> p_left = p; // not coloured yet
            unsigned colour = 0;         // current colour
            unsigned i = 0;              // position in p_bounds

            // while we've things left to colour
            while (! p_left.empty()) {
                // next colour
                ++colour;
                // things that can still be given this colour
                FixedBitSet<n_words_> q = p_left;

                // while we can still give something this colour
                while (! q.empty()) {
                    // first thing we can colour
                    int v = q.first_set_bit();
                    p_left.unset(v);
                    q.unset(v);

                    // can't give anything adjacent to this the same colour
                    graph.intersect_with_row_complement(v, q);

                    // record in result
                    p_bounds[i] = colour;
                    p_order[i] = v;
                    ++i;
                }
            }
        }

        auto colour_class_order_defer1(
                const FixedBitSet<n_words_> & p,
                std::array<unsigned, n_words_ * bits_per_word> & p_order,
                std::array<unsigned, n_words_ * bits_per_word> & p_bounds) -> void
        {
            FixedBitSet<n_words_> p_left = p; // not coloured yet
            unsigned colour = 0;        // current colour
            unsigned i = 0;             // position in p_bounds

            unsigned d = 0;             // number deferred
            std::array<unsigned, n_words_ * bits_per_word> defer;

            // while we've things left to colour
            while (! p_left.empty()) {
                // next colour
                ++colour;
                // things that can still be given this colour
                FixedBitSet<n_words_> q = p_left;

                // while we can still give something this colour
                unsigned number_with_this_colour = 0;
                while (! q.empty()) {
                    // first thing we can colour
                    int v = q.first_set_bit();
                    p_left.unset(v);
                    q.unset(v);

                    // can't give anything adjacent to this the same colour
                    graph.intersect_with_row_complement(v, q);

                    // record in result
                    p_bounds[i] = colour;
                    p_order[i] = v;
                    ++i;
                    ++number_with_this_colour;
                }

                if (1 == number_with_this_colour) {
                    --i;
                    --colour;
                    defer[d++] = p_order[i];
                }
            }

            for (unsigned n = 0 ; n < d ; ++n) {
                ++colour;
                p_order[i] = defer[n];
                p_bounds[i] = colour;
                i++;
            }
        }

        auto colour_class_order_sort(
                const FixedBitSet<n_words_> & p,
                std::array<unsigned, n_words_ * bits_per_word> & p_order,
                std::array<unsigned, n_words_ * bits_per_word> & p_bounds) -> void
        {
            FixedBitSet<n_words_> p_left = p; // not coloured yet
            std::vector<std::vector<unsigned> > colour_classes;

            // while we've things left to colour
            while (! p_left.empty()) {
                // next colour
                colour_classes.push_back({});

                // things that can still be given this colour
                FixedBitSet<n_words_> q = p_left;

                // while we can still give something this colour
                while (! q.empty()) {
                    // first thing we can colour
                    int v = q.first_set_bit();
                    p_left.unset(v);
                    q.unset(v);

                    // can't give anything adjacent to this the same colour
                    graph.intersect_with_row_complement(v, q);

                    // record in result
                    colour_classes.back().push_back(v);
                }
            }

            std::stable_sort(colour_classes.begin(), colour_classes.end(), [] (const auto & a, const auto & b) {
                    return a.size() > b.size();
                    });

            unsigned colour = 0;         // current colour
            unsigned i = 0;              // position in p_bounds

            for (auto & c : colour_classes) {
                ++colour;
                for (auto & v : c) {
                    p_order[i] = v;
                    p_bounds[i] = colour;
                    ++i;
                }
            }
        }

        auto expand(
                std::vector<unsigned> & c,
                FixedBitSet<n_words_> & p
                ) -> void
        {
            ++nodes;

            // initial colouring
            std::array<unsigned, n_words_ * bits_per_word> p_order;
            std::array<unsigned, n_words_ * bits_per_word> p_bounds;

            switch (params.how_much_sorting) {
                case Params::no_sorting:
                    colour_class_order(p, p_order, p_bounds);
                    break;

                case Params::defer1:
                    colour_class_order_defer1(p, p_order, p_bounds);
                    break;

                case Params::full_sort:
                    colour_class_order_sort(p, p_order, p_bounds);
                    break;
            }

            if (params.measure_kendall_tau) {
                std::vector<unsigned> sizes;
                unsigned count = 0;
                for (unsigned i = 0 ; i < p.popcount() ; ++i) {
                    ++count;
                    if (i > 0 && p_bounds[i - 1] != p_bounds[i]) {
                        sizes.push_back(count);
                        count = 0;
                    }
                }
                if (0 != count)
                    sizes.push_back(count);

                if (params.shuffle_before_tau)
                    random_shuffle(sizes.begin(), sizes.end());

                std::cerr << "a <- c(";
                for (unsigned i = 0 ; i < sizes.size() ; ++i) {
                    if (0 != i)
                        std::cerr << ", ";
                    std::cerr << sizes[i];
                }
                std::cerr << "); b <- c(";

                sort(sizes.begin(), sizes.end(), [] (auto a, auto b) { return a > b; });

                unsigned rank = p.popcount();
                for (unsigned i = 0 ; i < sizes.size() ; ++i) {
                    if (i > 0 && sizes[i - 1] != sizes[i])
                        --rank;
                    if (0 != i)
                        std::cerr << ", ";
                    std::cerr << rank;
                }

                std::cerr << "); invisible(cat(paste(" << p_bounds[p.popcount() - 1] << ", cor(a, b, method=\"kendall\"), \"\\n\")));" << std::endl;
            }

            // for each v in p... (v comes later)
            for (int n = p.popcount() - 1 ; n >= 0 ; --n) {
                // bound, timeout or early exit?
                if (c.size() + p_bounds[n] <= incumbent.value || params.abort->load())
                    return;

                auto v = p_order[n];

                // consider taking v
                c.push_back(v);

                // filter p to contain vertices adjacent to v
                FixedBitSet<n_words_> new_p = p;
                graph.intersect_with_row(v, new_p);

                if (! new_p.empty())
                    expand(c, new_p);
                else
                    incumbent.update(c);

                // now consider not taking v
                c.pop_back();
                p.unset(v);
            }
        }

        auto run() -> Result
        {
            std::vector<unsigned> c;
            c.reserve(graph.size());

            FixedBitSet<n_words_> p;
            p.set_up_to(graph.size());

            incumbent.value = params.prime;

            // go!
            expand(c, p);

            Result result;
            result.nodes = nodes;
            for (auto & v : incumbent.c)
                result.clique.insert(order[v]);

            return result;
        }
    };

    template <template <unsigned> class SGI_>
    struct Apply
    {
        template <unsigned size_, typename> using Type = SGI_<size_>;
    };
}

auto clique(const Graph & graph, const Params & params) -> Result
{
    return select_graph_size<Apply<Clique>::template Type, Result>(AllGraphSizes(), graph, params);
}

