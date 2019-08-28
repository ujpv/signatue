#include "signature.h"
#include "utils.h"

#include <fstream>
#include <iostream>
#include <thread>

void print_progress(size_t total, size_t read)
{
    std::cout << "\rRead " << read << " blocks of " << total <<"...";
    if (total == read)
        std::cout << "Done\n";
}

int main(int argc, const char* argv[])
{
    if (auto args = utils::parse_args(argc, argv)) {
        if (auto file = std::ifstream(args->input, std::ios::binary)) {
            std::string sign;
            try {
                sign = signature(file,
                                 args->block_size,
                                 std::thread::hardware_concurrency(),
                                 print_progress);
            } catch (std::exception& e) {
                std::cerr << "Failed. Reason: " << e.what() << '\n';
                return 1;
            } catch (...) {
                std::cerr << "Unknown error\n";
                return -1;
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
