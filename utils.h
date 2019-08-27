#pragma once

#include <string>
#include <boost/optional.hpp>

namespace utils {

struct arguments {
    std::string input;
    std::string output;
    size_t block_size;
};

boost::optional<arguments> parse_args(int argc, const char *argv[]);

} // namespace utils
