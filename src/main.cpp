#include <algorithm>
#include <atomic>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <sys/stat.h>

constexpr size_t NUM_THREADS = 128;

// forward declaration.
class WalkDir;
class WalkDirTask;

struct WalkDirArgs {
  std::shared_ptr<WalkDirTask> parent;
  std::filesystem::path path;
  size_t depth;
};

struct WalkDirEntry {
  WalkDirEntry(const std::filesystem::path &path, size_t depth)
      : WalkDirEntry(std::filesystem::directory_entry(path), depth) {}

  /// @brief creates directory entry from filesystem.
  ///
  /// - initializes `path`, `hash`, `depth`, `mode`, `ctime`, `mtime`, `atime`
  /// - initializes `size` for non-directories.
  /// - for directories, `size`, `width`, `length` will be update later.
  WalkDirEntry(std::filesystem::directory_entry entry, size_t depth)
      : path(entry.path()), hash(std::hash<std::filesystem::path>{}(path)),
        size(0), depth(depth), width(0), length(0),
        mode(entry.symlink_status().type()), ctime(), mtime(), atime() {
    // initializes `ctime`, `atime`, `mtime`
    struct stat buf;
    lstat(path.c_str(), &buf);
    ctime = buf.st_ctim;
    atime = buf.st_atim;
    mtime = buf.st_mtim;

    // initializes `size` for non-directories.
    if (mode != std::filesystem::file_type::directory) {
      size = entry.file_size();
    }
  }

  /// @brief absolute path to the directory entry.
  std::filesystem::path path;
  /// @brief pre-computed hash of @c path.
  uint64_t hash;
  /// @brief size in bytes.
  ///
  /// - represents the file size for non-directories.
  /// - represents the sum sizes of the child entries for directories.
  size_t size;
  /// @brief distance from root path to current path.
  ///
  /// - `0` for root path.
  size_t depth;
  /// @brief number of child entries in a directory.
  ///
  /// - only available for directories.
  /// - 0 for empty directory.
  size_t width;
  /// @brief distance from current path to the deepest entry.
  ///
  /// - only available for directories.
  /// - 0 for empty directory.
  /// - 1 for directory that contains files but does not have sub-directories.
  size_t length;
  /// @brief file type.
  /// @see <sys/stat.h>
  /// @see <https://en.cppreference.com/w/cpp/filesystem/file_type>
  std::filesystem::file_type mode;
  /// @brief last created time, using format "YYYY-MM-DDThh:mm:ssZ".
  std::timespec ctime;
  /// @brief last modified time, using format "YYYY-MM-DDThh:mm:ssZ".
  std::timespec mtime;
  /// @brief last accessed time, using format "YYYY-MM-DDThh:mm:ssZ".
  std::timespec atime;
};

class WalkDirTask : public std::enable_shared_from_this<WalkDirTask> {
public:
  /// @note use @c walkdir->spawn(...) instead.
  WalkDirTask(WalkDir &walkdir, std::shared_ptr<WalkDirTask> parent,
              const std::filesystem::path &path, size_t depth);

  ~WalkDirTask();

  /// @brief callback method, called by destructor of @c WalkDirTask.
  void on_child_submit(const WalkDirEntry &child);

  /// @brief returns an iterator over all the entires in the directory.
  ///
  /// @note returns an empty iterator for non-directories.
  std::filesystem::directory_iterator begin() const;
  std::filesystem::directory_iterator end() const;

  /// @brief spawns child task args.
  WalkDirArgs spawn(std::filesystem::directory_iterator iter);

private:
  WalkDir &m_walkdir;
  /// @brief pointer to parent directory.
  std::shared_ptr<WalkDirTask> m_parent;
  /// @brief result of the task.
  WalkDirEntry m_entry;
};

class WalkDir {
public:
  WalkDir(const std::filesystem::path &rootpath);
  ~WalkDir();

  /// @brief spawns a new task with given @c args.
  std::shared_ptr<WalkDirTask> spwan(WalkDirArgs args);

  /// @brief callback method, called by destructor of @c WalkDirTask.
  void on_entry_submit(const WalkDirEntry &entry);

private:
  std::vector<std::thread> m_threads;
  // element is formatted `WalkDirEntry`.
  std::deque<WalkDirArgs> m_queue;
  std::mutex m_queue_mutex;
  std::atomic_bool m_shutdown;
  std::ofstream m_output;
  std::mutex m_output_mutex;
};

WalkDir::WalkDir(const std::filesystem::path &rootpath)
    : m_shutdown(false), m_output("output.dat") {
  // pushes `rootpath` to `queue`.
  m_queue.push_front(WalkDirArgs{
      .parent = nullptr,
      .path = rootpath,
      .depth = 0,
  });

  // spawn threads.
  m_threads.reserve(NUM_THREADS);
  for (size_t i = 0; i < NUM_THREADS; ++i) {
    m_threads.emplace_back([this] {
      while (true) {

        std::optional<WalkDirArgs> opt_args = std::nullopt;

        {
          // dequeue.
          std::lock_guard<std::mutex> lock(m_queue_mutex);
          if (!m_queue.empty()) {
            opt_args = std::move(m_queue.back());
            m_queue.pop_back();
          }
        }

        if (opt_args.has_value()) {
          // handles task.
          auto task = spwan(std::move(opt_args.value()));
          for (auto iter = task->begin(); iter != task->end(); ++iter) {
            auto child = task->spawn(iter);
            // enqueue.
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_queue.push_front(std::move(child));
          }
        } else if (m_shutdown) {
          // exits thread.
          return;
        } else {
          // yields thread.
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
    });
  }
}

WalkDir::~WalkDir() {
  m_shutdown = true;
  for (auto &thread : m_threads) {
    thread.join();
  }
}

void WalkDir::on_entry_submit(const WalkDirEntry &entry) {

  char ctimebuf[24];
  std::strftime(ctimebuf, sizeof(ctimebuf), "%FT%TZ",
                std::localtime(&entry.ctime.tv_sec));

  char mtimebuf[24];
  std::strftime(mtimebuf, sizeof(mtimebuf), "%FT%TZ",
                std::localtime(&entry.mtime.tv_sec));

  char atimebuf[24];
  std::strftime(atimebuf, sizeof(atimebuf), "%FT%TZ",
                std::localtime(&entry.atime.tv_sec));

  auto formatted = std::format(
      "{:016x}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n", entry.hash, entry.size,
      entry.depth, entry.width, entry.length, static_cast<int>(entry.mode),
      ctimebuf, mtimebuf, atimebuf, entry.path.string());

  {
    std::lock_guard lock(m_output_mutex);
    m_output << formatted;
  }
}

std::shared_ptr<WalkDirTask> WalkDir::spwan(WalkDirArgs args) {
  return std::make_shared<WalkDirTask>(*this, args.parent, args.path,
                                       args.depth);
}

WalkDirTask::WalkDirTask(WalkDir &walkdir, std::shared_ptr<WalkDirTask> parent,
                         const std::filesystem::path &path, size_t depth)
    : m_walkdir(walkdir), m_parent(std::move(parent)), m_entry(path, depth) {}

WalkDirTask::~WalkDirTask() {
  if (m_parent != nullptr) {
    m_parent->on_child_submit(m_entry);
  }
  m_walkdir.on_entry_submit(m_entry);
}

void WalkDirTask::on_child_submit(const WalkDirEntry &child) {
  // updates `width`, `size`, `length` fields.
  m_entry.width += 1;
  m_entry.size += child.size;
  m_entry.length = std::max(m_entry.length, child.length + 1);
}

std::filesystem::directory_iterator WalkDirTask::begin() const {
  if (m_entry.mode == std::filesystem::file_type::directory) {
    return std::filesystem::directory_iterator(m_entry.path);
  } else {
    return std::filesystem::directory_iterator();
  }
}

std::filesystem::directory_iterator WalkDirTask::end() const {
  return std::filesystem::directory_iterator();
}

WalkDirArgs WalkDirTask::spawn(std::filesystem::directory_iterator iter) {
  return WalkDirArgs{
      .parent = shared_from_this(),
      .path = iter->path(),
      .depth = m_entry.depth + 1,
  };
}

int main(int argc, const char **argv) {
  const char *rootpath = ".";

  if (argc >= 2) {
    rootpath = argv[1];
  }

  WalkDir walkdir(rootpath);
  return 0;
}