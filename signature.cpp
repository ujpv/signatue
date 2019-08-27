#include "signature.h"
#include "md5.h"

#include <boost/lockfree/queue.hpp>
#include <boost/optional.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {

class jobs_pool {
private:
    struct file_chunk {
        std::vector<char> buffer;
        size_t number;
    };

public:
    struct job_handler {
        std::vector<char>& buffer;
        size_t& number;
        const size_t index;
    };

    explicit jobs_pool(size_t size, size_t buffer_size)
        : chunks_(size, {std::vector<char>(buffer_size, '\0'), 0})
        , jobs_(size)
        , ready_(size)
        , active_(true)
    {
        for (size_t i = 0; i < size; ++i)
            ready_.push(i);
    }

    job_handler allocate()
    {
        size_t index{};
        while (!ready_.pop(index));
        auto& block = chunks_[index];
        return {block.buffer, block.number, index};
    }

    void free(const job_handler& block)
    {
        while (!ready_.push(block.index));
    }

    boost::optional<job_handler> get()
    {
        size_t index{};
        while (active_ || !jobs_.empty()) {
            if (jobs_.pop(index)) {
                auto& block = chunks_[index];
                return boost::make_optional(
                    job_handler{block.buffer, block.number, index});
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }

        return boost::none;
    }

    void enqueue(const job_handler& block)
    {
        while (!jobs_.push(block.index));
    }

    void terminate() { active_ = false; }

private:
    // keeps memory for file chunks
    std::vector<file_chunk> chunks_;
    // prepared jobs to process
    boost::lockfree::queue<size_t, boost::lockfree::fixed_sized<true>> jobs_;
    // ready to use
    boost::lockfree::queue<size_t, boost::lockfree::fixed_sized<true>> ready_;
    std::atomic_bool active_;
};

class thread_pool {
public:
    thread_pool(size_t size, const std::function<void(void)>& func)
    {
        for (size_t i = 0; i < size; ++i)
            threads_.emplace_back(func);
    }

    void join()
    {
        for (auto& thread: threads_)
            thread.join();
    }

    ~thread_pool()
    {
        try {
            for (auto& thread: threads_) {
                if (thread.joinable())
                    thread.join();
            }
        } catch (...) {
            // suppress
        }
    }

private:
    std::vector<std::thread> threads_;
};

}

std::string signature(
    std::istream& input,
    size_t block_size,
    size_t thread_count,
    const std::function<void(size_t, size_t)>& progress_callback)
{
    jobs_pool jobs(thread_count * 2, block_size);

    // state
    std::atomic_bool aborted(false);
    input.seekg(0, std::istream::end);
    const size_t file_size = static_cast<size_t>(input.tellg());
    input.seekg(0, std::istream::beg);
    const size_t blocks_count = (file_size + block_size - 1) / block_size;
    auto abort = [&aborted](const std::string& msg) {
        aborted = true;
        throw std::runtime_error(msg);
    };

    // result
    std::string sign(blocks_count * MD5::HASH_LEN, '\0');

    // consumers pool
    thread_pool workers(thread_count, [&]() {
        while (!aborted) {
            auto block = jobs.get();
            if (!block)
                return;

            MD5 md5;
            md5.update(block->buffer.data(), block->buffer.size());
            md5.finalize();
            md5.hexdigest(sign, block->number * MD5::HASH_LEN);
            jobs.free(*block);
        }
    });

    // read file
    std::vector<char> buffer(block_size, '\0');
    size_t blocks_read = 0;
    while (blocks_count != blocks_read && !aborted) {
        if (blocks_read + 1 == blocks_count)
            buffer.resize(file_size % block_size);

        input.read(buffer.data(), std::streamsize(buffer.size()));
        if (input.fail())
            abort("Read error");

        auto block = jobs.allocate();
        std::swap(block.buffer, buffer);
        block.number = blocks_read++;
        jobs.enqueue(block);
        if (progress_callback)
            progress_callback(blocks_count, blocks_read);
    }

    jobs.terminate();
    // throw exception
    workers.join();

    return sign;
}

