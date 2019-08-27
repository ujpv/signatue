#include "signature.h"
#include "md5.h"

#include <boost/lockfree/queue.hpp>
#include <boost/optional.hpp>

#include <atomic>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace {

class jobs_pool {
private:
    struct block {
        std::string buffer;
        size_t number;
    };

public:
    struct block_handler {
        std::string &buffer;
        size_t &number;
        const size_t index;
    };

    explicit jobs_pool(size_t size, size_t buffer_size)
        : blocks_(size, {std::string(buffer_size, '\0'), 0})
        , jobs_(size)
        , ready_(size)
        , active_(true)
    {
        for (size_t i = 0; i < size; ++i)
            ready_.push(i);
    }

    block_handler allocate()
    {
        size_t index{};
        while (!ready_.pop(index));
        auto& block = blocks_[index];
        return {block.buffer, block.number, index};
    }

    void free(const block_handler& block)
    {
        while (!ready_.push(block.index));
    }

    boost::optional<block_handler> get()
    {
        size_t index{};
        while (active_ || !jobs_.empty()) {
            if (jobs_.pop(index)) {
                auto& block = blocks_[index];
                return boost::make_optional(
                    block_handler{block.buffer, block.number, index});
            }
        }

        return boost::none;
    }

    void enqueue(const block_handler& block)
    {
        while (!jobs_.push(block.index));
    }

    void terminate() { active_ = false; }

private:
    // keep block memory
    std::vector<block> blocks_;
    // contains data to process
    boost::lockfree::queue<size_t, boost::lockfree::fixed_sized<true>> jobs_;
    // ready to use
    boost::lockfree::queue<size_t, boost::lockfree::fixed_sized<true>> ready_;
    std::atomic_bool active_;
};

class thread_pool {
public:
    explicit thread_pool(size_t size, std::function<void(void)> func)
    {
        for (size_t i = 0; i < size; ++i)
            threads_.emplace_back(func);
    }

    // wait for the threads termination and rethrow the first exception
    void join()
    try {
        for (auto& thread: threads_)
            thread.join();
    } catch (...) {
        for (auto& thread: threads_) {
            if (thread.joinable())
                try { thread.join(); } catch(...) {}
        }

        throw;
    }


private:
    std::vector<std::thread> threads_;
};

}

std::string signatue(
    std::istream &input,
    size_t block_size,
    size_t thread_count,
    std::function<void(size_t, size_t)> progress_callback)
{
    jobs_pool jobs(thread_count * 2, block_size);

    // state
    std::atomic_bool reading(true);
    std::atomic_bool aborted(false);
    input.seekg(0, input.end);
    const size_t file_size = static_cast<size_t>(input.tellg());
    input.seekg(0, input.beg);
    const size_t blocks_count = (file_size + block_size - 1) / block_size;

    // result
    std::string sign(blocks_count * MD5::HASH_LEN, '\0');

    // consumers pool
    thread_pool workers(thread_count, [&]() {
        while (!aborted) {
            auto block = jobs.get();
            if (!block)
                return;

            if (block->number < blocks_count) {
                MD5(block->buffer).hexdigest(&sign[0] + block->number * MD5::HASH_LEN);
                jobs.free(*block);
            } else {
                aborted = true;
                throw std::runtime_error("Wrong size. File has been chaned during reading");
            }
        }
        return;
    });

    // read file - procucer
    std::string buffer(block_size, '\0');
    size_t blocks_read = 0;
    while (!input.eof()) {
        input.read(&buffer[0], std::streamsize(buffer.size()));
        buffer.resize(size_t(input.gcount()));
        auto block = jobs.allocate();
        std::swap(block.buffer, buffer);
        block.number = blocks_read++;
        jobs.enqueue(block);
        if (progress_callback)
            progress_callback(blocks_count, blocks_read);
    }

    jobs.terminate();
    reading = false;
    // throw exception
    workers.join();

    return sign;
}

