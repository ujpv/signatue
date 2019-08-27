#pragma once

#include <string>
#include <istream>
#include <exception>
#include <functional>

std::string signatue(
    std::istream& input,
    size_t block_size,
    size_t thread_count,
    std::function<void(size_t, size_t)> progress_callback = {}
    );
