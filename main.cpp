#include "signature.h"
#include "utils.h"

#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, const char *argv[])
{
    if (auto args = utils::parse_args(argc, argv)) {
        if (auto file = std::ifstream(args->input, std::ios::binary)) {
            std::string sign;
            try {
                sign = signature(file,
                                 args->block_size,
                                 std::thread::hardware_concurrency());
            } catch (std::exception& e) {
                std::cerr << "Failed. Reason: " << e.what() << '\n';
            }
            std::ofstream sign_file(args->output);
            sign_file << sign;
            if (!sign_file)
                std::cerr << "Unable to save file " << args->output;
        } else {
            std::cerr << "File " << args->input << " not found.\n";
        }
    }

    return 0;
}
