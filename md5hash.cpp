#include <openssl/md5.h>

#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

struct Task {
    std::size_t index;
    fs::path path;
};

struct Result {
    bool ok;
    std::string md5hex;
    std::string err;
};

static std::string md5_file_hex(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open file");

    MD5_CTX ctx;
    MD5_Init(&ctx);

    static constexpr std::size_t BUF_SZ = 1 << 20; // 1 MB
    std::vector<unsigned char> buf(BUF_SZ);

    while (in) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got > 0) MD5_Update(&ctx, buf.data(), static_cast<std::size_t>(got));
    }

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);

    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

static void collect_files_from_arg(const fs::path& arg, std::vector<fs::path>& out) {
    std::error_code ec;

    if (!fs::exists(arg, ec)) {
        std::cerr << "Warning: path not found: " << arg.string() << "\n";
        return;
    }

    if (fs::is_regular_file(arg, ec)) {
        out.push_back(arg);
        return;
    }

    if (fs::is_directory(arg, ec)) {
        fs::recursive_directory_iterator it(arg, fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator end;
        for (; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            std::error_code ec2;
            if (fs::is_regular_file(it->path(), ec2)) out.push_back(it->path());
        }
        return;
    }

    // Other types (symlink, device, etc.)
    std::cerr << "Warning: skipping non-regular path: " << arg.string() << "\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "USAGE: md5hash <directory/file> [more directories/files] [--threads N]\n";
        return 1;
    }

    // Parse args and optional --threads
    std::vector<std::string> rawArgs;
    rawArgs.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) rawArgs.emplace_back(argv[i]);

    std::size_t threads = 8; // requirement: at least 8
    std::vector<fs::path> inputs;

    for (std::size_t i = 0; i < rawArgs.size(); ++i) {
        if (rawArgs[i] == "--threads") {
            if (i + 1 >= rawArgs.size()) {
                std::cerr << "Error: --threads requires a number\n";
                return 1;
            }
            try {
                std::size_t n = static_cast<std::size_t>(std::stoul(rawArgs[i + 1]));
                threads = std::max<std::size_t>(8, n);
            } catch (...) {
                std::cerr << "Error: invalid --threads value\n";
                return 1;
            }
            ++i;
        } else {
            inputs.emplace_back(rawArgs[i]);
        }
    }

    if (inputs.empty()) {
        std::cerr << "Error: no input paths provided\n";
        return 1;
    }

    // Collect files
    std::vector<fs::path> files;
    for (const auto& p : inputs) collect_files_from_arg(p, files);

    if (files.empty()) {
        std::cerr << "No files found.\n";
        return 1;
    }

    // Deterministic: sort paths
    std::sort(files.begin(), files.end(),
              [](const fs::path& a, const fs::path& b) { return a.string() < b.string(); });

    // Shared structures
    std::queue<Task> q;
    q = std::queue<Task>();
    for (std::size_t i = 0; i < files.size(); ++i) q.push(Task{i, files[i]});

    std::vector<std::optional<Result>> results(files.size());
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;

    auto worker = [&]() {
        while (true) {
            Task t;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [&]() { return !q.empty() || done; });
                if (done && q.empty()) return;
                t = q.front();
                q.pop();
            }

            Result r;
            try {
                r.ok = true;
                r.md5hex = md5_file_hex(t.path);
            } catch (const std::exception& e) {
                r.ok = false;
                r.err = e.what();
            }

            {
                std::lock_guard<std::mutex> lock(mtx);
                results[t.index] = r;
            }
            cv.notify_all();
        }
    };

    // Start workers
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (std::size_t i = 0; i < threads; ++i) pool.emplace_back(worker);

    // Kick workers
    cv.notify_all();

    // Print progressively, but deterministically (index order)
    std::size_t next = 0;
    while (next < files.size()) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() { return results[next].has_value(); });

        const auto& res = results[next].value();
        const auto& path = files[next];

        if (res.ok) {
            std::cout << path.filename().string() << "\t" << res.md5hex << "\n";
        } else {
            std::cout << path.filename().string() << "\tERROR: " << res.err << "\n";
        }
        ++next;
    }

    // Shutdown
    {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
    }
    cv.notify_all();

    for (auto& th : pool) th.join();
    return 0;
}
