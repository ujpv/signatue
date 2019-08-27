#pragma once

#include <string>
#include <istream>
#include <exception>
#include <functional>

std::string signature(
    std::istream& input,
    size_t block_size,
    size_t thread_count,
    const std::function<void(size_t, size_t)>& progress_callback = {}
    );
