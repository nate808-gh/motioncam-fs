#include "linux/FuseFileSystemImpl_Linux.h"
#include "VirtualFileSystemImpl_MCRAW.h"
#include "LRUCache.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <BS_thread_pool.hpp>
#include <fuse.h>
#include <atomic>
#include <thread>

// Logging
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace fs = boost::filesystem;

namespace motioncam {

constexpr auto CACHE_SIZE = 1024 * 1024 * 1024;
constexpr auto IO_THREADS = 4;

namespace {
void setupLogging() {
    try {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/logfile.txt", true));
        auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
        spdlog::set_default_logger(logger);

#ifdef NDEBUG
        spdlog::set_level(spdlog::level::info);
#else
        spdlog::set_level(spdlog::level::debug);
#endif
        spdlog::flush_on(spdlog::level::info);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log init failed: " << ex.what() << std::endl;
    }
}
}

struct FuseContext {
    VirtualFileSystemImpl_MCRAW* fs;
    std::atomic_int nextFileHandle;
};

class Session {
public:
    Session(const std::string& srcFile, const std::string& mountPath, VirtualFileSystemImpl_MCRAW* fs);
    ~Session();

    VirtualFileSystemImpl_MCRAW* getFileSystem() const { return mFs; }
    void shutdown();

private:
    void init(VirtualFileSystemImpl_MCRAW* fs);
    void fuseMain();

    static void* fuseInit(struct fuse_conn_info* conn);
    static void fuseDestroy(void* privateData);
    static int fuseRelease(const char* path, struct fuse_file_info* fi);
    static int fuseGetattr(const char* path, struct stat* stbuf);
    static int fuseReaddir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
    static int fuseOpen(const char* path, struct fuse_file_info* fi);
    static int fuseRead(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);

private:
    std::string mSrcFile;
    std::string mMountPath;
    VirtualFileSystemImpl_MCRAW* mFs;
    struct fuse* mFuse;
    struct fuse_chan* mChan;
    std::unique_ptr<std::thread> mThread;
};

Session::Session(const std::string& srcFile, const std::string& mountPath, VirtualFileSystemImpl_MCRAW* fs)
    : mSrcFile(srcFile),
      mMountPath(mountPath),
      mFs(fs),
      mFuse(nullptr),
      mChan(nullptr)
{
    init(fs);
}

Session::~Session() {
    shutdown();
}

    void Session::shutdown() {
    if (mFuse) {
        fuse_exit(mFuse);  // ask the fuse loop to stop
    }

    // do a background join so GUI does not freeze
    if (mThread && mThread->joinable()) {
        std::thread([thr = std::move(mThread)]() mutable {
            if (thr && thr->joinable()) {
                thr->join();
            }
        }).detach();
    }

    if (mChan) {
        fuse_unmount(mMountPath.c_str(), mChan);
        mChan = nullptr;
    }

    if (mFuse) {
        fuse_destroy(mFuse);
        mFuse = nullptr;
    }

    try {
        if (fs::exists(mMountPath) && fs::is_directory(mMountPath) && fs::is_empty(mMountPath)) {
            fs::remove(mMountPath);
            spdlog::info("Removed empty mount directory: {}", mMountPath);
        }
    } catch (const fs::filesystem_error& ex) {
        spdlog::warn("Unable to remove mount directory: {}", ex.what());
    }

    spdlog::debug("Session shutdown complete for {}", mSrcFile);
}


void Session::init(VirtualFileSystemImpl_MCRAW* fs) {
    struct fuse_operations ops = {};
    ops.init      = fuseInit;
    ops.destroy   = fuseDestroy;
    ops.release   = fuseRelease;
    ops.getattr   = fuseGetattr;
    ops.readdir   = fuseReaddir;
    ops.open      = fuseOpen;
    ops.read      = fuseRead;

    struct fuse_args args = FUSE_ARGS_INIT(0, nullptr);
    fuse_opt_add_arg(&args, "-f");
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "ro");

    auto* context = new FuseContext();
    context->fs = fs;
    context->nextFileHandle = 0;

    mChan = fuse_mount(mMountPath.c_str(), &args);
    if (!mChan) {
        delete context;
        throw std::runtime_error("fuse_mount failed");
    }

    mFuse = fuse_new(mChan, &args, &ops, sizeof(ops), context);
    if (!mFuse) {
        fuse_unmount(mMountPath.c_str(), mChan);
        delete context;
        throw std::runtime_error("fuse_new failed");
    }

    mThread = std::make_unique<std::thread>(&Session::fuseMain, this);
}

void Session::fuseMain() {
    int res = fuse_loop(mFuse);
    spdlog::info("FUSE exited with code {}", res);
}

void* Session::fuseInit(struct fuse_conn_info* conn) {
    return fuse_get_context()->private_data;
}

void Session::fuseDestroy(void* privateData) {
    auto* context = reinterpret_cast<FuseContext*>(privateData);
    delete context;
}

int Session::fuseGetattr(const char* path, struct stat* stbuf) {
    auto* context = reinterpret_cast<FuseContext*>(fuse_get_context()->private_data);
    memset(stbuf, 0, sizeof(struct stat));

    std::string p(path);
    if (p == "/" || p == "//") {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    auto entry = context->fs->findEntry(p);
    if (!entry.has_value())
        return -ENOENT;

    if (entry->type == EntryType::DIRECTORY_ENTRY) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size = 4096;
    } else {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = entry->size;
    }
    return 0;
}

int Session::fuseReaddir(const char* path, void* buf, fuse_fill_dir_t filler, off_t, struct fuse_file_info*) {
    auto* context = reinterpret_cast<FuseContext*>(fuse_get_context()->private_data);
    std::string p(path);

    if (p == "/" || p == "//") {
        filler(buf, ".", nullptr, 0);
        filler(buf, "..", nullptr, 0);
        auto files = context->fs->listFiles("/");
        for (const auto& entry : files)
            filler(buf, entry.getFullPath().c_str(), nullptr, 0);
        return 0;
    }
    return -ENOENT;
}

int Session::fuseOpen(const char* path, struct fuse_file_info* fi) {
    auto* context = reinterpret_cast<FuseContext*>(fuse_get_context()->private_data);
    auto entry = context->fs->findEntry(path);
    if (!entry.has_value())
        return -ENOENT;

    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    fi->fh = ++context->nextFileHandle;
    return 0;
}

int Session::fuseRead(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info*) {
    auto* context = reinterpret_cast<FuseContext*>(fuse_get_context()->private_data);
    auto entry = context->fs->findEntry(path);
    if (!entry.has_value())
        return -ENOENT;

    return context->fs->readFile(
        entry.value(),
        offset,
        size,
        buf,
        [](auto, auto) {},
        false);
}

int Session::fuseRelease(const char*, struct fuse_file_info*) {
    return 0;
}

FuseFileSystemImpl_Linux::FuseFileSystemImpl_Linux()
    : mNextMountId(0),
      mIoThreadPool(std::make_unique<BS::thread_pool<>>(IO_THREADS)),
      mProcessingThreadPool(std::make_unique<BS::thread_pool<>>()),
      mCache(std::make_unique<LRUCache>(CACHE_SIZE))
{
    setupLogging();
}

FuseFileSystemImpl_Linux::~FuseFileSystemImpl_Linux() {
    mMountedFiles.clear();
    mIoThreadPool->wait();
    mProcessingThreadPool->wait();
    spdlog::info("Destroyed FuseFileSystemImpl_Linux()");
}

MountId FuseFileSystemImpl_Linux::mount(
    FileRenderOptions options, int draftScale,
    const std::string& srcFile, const std::string& dstPath)
{
    fs::path srcPath(srcFile);
    std::string ext = srcPath.extension().string();

    if (boost::iequals(ext, ".mcraw")) {
        auto mountId = mNextMountId++;
        try {
            auto* fs = new VirtualFileSystemImpl_MCRAW(
                *mIoThreadPool,
                *mProcessingThreadPool,
                *mCache,
                options,
                draftScale,
                srcFile);

            auto session = std::make_unique<Session>(srcFile, dstPath, fs);
            mMountedFiles[mountId] = std::move(session);
        }
        catch (std::runtime_error& e) {
            spdlog::error("mount error: {}", e.what());
            throw;
        }
        return mountId;
    }

    throw std::runtime_error("Unsupported format");
}

void FuseFileSystemImpl_Linux::unmount(MountId mountId) {
    auto it = mMountedFiles.find(mountId);
    if (it != mMountedFiles.end()) {
        it->second->shutdown();
        mMountedFiles.erase(it);
    }
}

void FuseFileSystemImpl_Linux::updateOptions(MountId mountId, FileRenderOptions options, int draftScale) {
    auto it = mMountedFiles.find(mountId);
    if (it != mMountedFiles.end()) {
        it->second->getFileSystem()->updateOptions(options, draftScale);
    }
}

} // namespace motioncam
