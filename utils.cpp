#include "utils.h"

#include <boost/program_options.hpp>

#include <iostream>

namespace utils {

boost::optional<arguments> parse_args(int argc, const char *argv[])
{
    namespace opts = boost::program_options;
    opts::options_description descr{"Options"};
    descr.add_options()
        ("help,h", "Help screen")
        ("input,i", opts::value<std::string>()->required(), "File name")
        ("output,o", opts::value<std::string>()->required(), "Output filename")
        ("block,b", opts::value<size_t>()->default_value(1024 * 1024), "Reading block size");

    opts::variables_map vm;
    try {
        opts::store(parse_command_line(argc, argv, descr), vm);
        if (vm.count("help")) {
            std::cerr << descr << '\n';
            return boost::none;
        }

        opts::notify(vm);
    } catch (std::exception& e) {
        std::cerr << "Unable to parse arguments. Reason:\n"
                  << e.what() << "\n\n" << descr;
        return boost::none;
    }

    return boost::make_optional<arguments>({vm["input"].as<std::string>(),
                                            vm["output"].as<std::string>(),
                                            vm["block"].as<size_t>()});
}

} // namespace utils
