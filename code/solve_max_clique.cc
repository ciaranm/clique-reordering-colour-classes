/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include "clique.hh"

#include <boost/program_options.hpp>
#include <boost/regex.hpp>

#include <iostream>
#include <fstream>
#include <exception>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>

namespace po = boost::program_options;

using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

namespace
{
    class GraphFileError :
        public std::exception
    {
        private:
            std::string _what;

        public:
            GraphFileError(const std::string & filename, const std::string & message) throw ();

            auto what() const throw () -> const char *;
    };

    GraphFileError::GraphFileError(const std::string & filename, const std::string & message) throw () :
        _what("Error reading graph file '" + filename + "': " + message)
    {
    }

    auto GraphFileError::what() const throw () -> const char *
    {
        return _what.c_str();
    }

    auto read_dimacs(const std::string & filename) -> Graph
    {
        Graph result;

        std::ifstream infile{ filename };
        if (! infile)
            throw GraphFileError{ filename, "unable to open file" };

        std::string line;
        while (std::getline(infile, line)) {
            if (line.empty())
                continue;

            /* Lines are comments, a problem description (contains the number of
             * vertices), or an edge. */
            static const boost::regex
                comment{ R"(c(\s.*)?)" },
                problem{ R"(p\s+(edge|col)\s+(\d+)\s+(\d+)?\s*)" },
                edge{ R"(e\s+(\d+)\s+(\d+)\s*)" };

            boost::smatch match;
            if (regex_match(line, match, comment)) {
                /* Comment, ignore */
            }
            else if (regex_match(line, match, problem)) {
                /* Problem. Specifies the size of the graph. Must happen exactly
                 * once. */
                if (0 != result.size)
                    throw GraphFileError{ filename, "multiple 'p' lines encountered" };
                result.size = std::stoi(match.str(2));
                result.edges.resize(result.size);
            }
            else if (regex_match(line, match, edge)) {
                /* An edge. DIMACS files are 1-indexed. We assume we've already had
                 * a problem line (if not our size will be 0, so we'll throw). */
                int a{ std::stoi(match.str(1)) }, b{ std::stoi(match.str(2)) };
                if (0 == a || 0 == b || unsigned(a) > result.size || unsigned(b) > result.size)
                    throw GraphFileError{ filename, "line '" + line + "' edge index out of bounds" };
                else if (a == b)
                    throw GraphFileError{ filename, "line '" + line + "' contains a loop" };
                result.edges[a - 1].insert(b - 1);
                result.edges[b - 1].insert(a - 1);
            }
            else
                throw GraphFileError{ filename, "cannot parse line '" + line + "'" };
        }

        if (! infile.eof())
            throw GraphFileError{ filename, "error reading file" };

        return result;
    }
}

/* Helper: return a function that runs the specified algorithm, dealing
 * with timing information and timeouts. */
template <typename Result_, typename Params_, typename Data_>
auto run_this_wrapped(const std::function<Result_ (const Data_ &, const Params_ &)> & func)
    -> std::function<Result_ (const Data_ &, Params_ &, bool &, int)>
{
    return [func] (const Data_ & data, Params_ & params, bool & aborted, int timeout) -> Result_ {
        /* For a timeout, we use a thread and a timed CV. We also wake the
         * CV up if we're done, so the timeout thread can terminate. */
        std::thread timeout_thread;
        std::mutex timeout_mutex;
        std::condition_variable timeout_cv;
        std::atomic<bool> abort;
        abort.store(false);
        params.abort = &abort;
        if (0 != timeout) {
            timeout_thread = std::thread([&] {
                    auto abort_time = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
                    {
                        /* Sleep until either we've reached the time limit,
                         * or we've finished all the work. */
                        std::unique_lock<std::mutex> guard(timeout_mutex);
                        while (! abort.load()) {
                            if (std::cv_status::timeout == timeout_cv.wait_until(guard, abort_time)) {
                                /* We've woken up, and it's due to a timeout. */
                                aborted = true;
                                break;
                            }
                        }
                    }
                    abort.store(true);
                    });
        }

        /* Start the clock */
        params.start_time = std::chrono::steady_clock::now();
        auto result = func(data, params);

        /* Clean up the timeout thread */
        if (timeout_thread.joinable()) {
            {
                std::unique_lock<std::mutex> guard(timeout_mutex);
                abort.store(true);
                timeout_cv.notify_all();
            }
            timeout_thread.join();
        }

        return result;
    };
}

/* Helper: return a function that runs the specified algorithm, dealing
 * with timing information and timeouts. */
template <typename Result_, typename Params_, typename Data_>
auto run_this(Result_ func(const Data_ &, const Params_ &)) -> std::function<Result_ (const Data_ &, Params_ &, bool &, int)>
{
    return run_this_wrapped(std::function<Result_ (const Data_ &, const Params_ &)>(func));
}

auto main(int argc, char * argv[]) -> int
{
    try {
        po::options_description display_options{ "Program options" };
        display_options.add_options()
            ("help",                                  "Display help information")
            ("timeout",            po::value<int>(),  "Abort after this many seconds")
            ;

        po::options_description all_options{ "All options" };
        all_options.add_options()
            ("file",    po::value<std::string>(), "Clique file")
            ;

        all_options.add(display_options);

        po::positional_options_description positional_options;
        positional_options
            .add("file", 1)
            ;

        po::variables_map options_vars;
        po::store(po::command_line_parser(argc, argv)
                .options(all_options)
                .positional(positional_options)
                .run(), options_vars);
        po::notify(options_vars);

        /* --help? Show a message, and exit. */
        if (options_vars.count("help")) {
            std::cout << "Usage: " << argv[0] << " [options] file" << std::endl;
            std::cout << std::endl;
            std::cout << display_options << std::endl;
            return EXIT_SUCCESS;
        }

        /* No input file specified? Show a message and exit. */
        if (! options_vars.count("file")) {
            std::cout << "Usage: " << argv[0] << " [options] file" << std::endl;
            return EXIT_FAILURE;
        }

        /* Figure out what our options should be. */
        Params params;

        params.connected = options_vars.count("connected");

        if (params.connected && ! options_vars.count("undirected")) {
            std::cerr << "Currently --connected requires --undirected" << std::endl;
            return EXIT_FAILURE;
        }

        /* Create graphs */
        auto graph = read_dimacs(options_vars["file"].as<std::string>());

        /* Do the actual run. */
        bool aborted = false;
        Result result;

        result = run_this(clique)(
                graph,
                params,
                aborted,
                options_vars.count("timeout") ? options_vars["timeout"].as<int>() : 0);

        /* Stop the clock. */
        auto overall_time = duration_cast<milliseconds>(steady_clock::now() - params.start_time);

        /* Display the results. */
        std::cout << result.clique.size() << " " << result.nodes;

        if (aborted)
            std::cout << " aborted";
        std::cout << std::endl;

        for (auto v : result.clique)
            std::cout << v << " ";
        std::cout << std::endl;

        std::cout << overall_time.count();
        if (! result.times.empty()) {
            for (auto t : result.times)
                std::cout << " " << t.count();
        }
        std::cout << std::endl;

        return EXIT_SUCCESS;
    }
    catch (const po::error & e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Try " << argv[0] << " --help" << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

