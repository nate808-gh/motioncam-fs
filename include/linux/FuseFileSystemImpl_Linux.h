#pragma once

#include <map>
#include <memory>
#include "IFuseFileSystem.h"
#include <BS_thread_pool.hpp>


namespace motioncam {

    struct Session;
    class LRUCache;

    class FuseFileSystemImpl_Linux : public IFuseFileSystem
    {
    public:
        FuseFileSystemImpl_Linux();
        ~FuseFileSystemImpl_Linux();

        MountId mount(FileRenderOptions options, int draftScale, const std::string& srcFile, const std::string& dstPath) override;
        void unmount(MountId mountId) override;
        void updateOptions(MountId mountId, FileRenderOptions options, int draftScale) override;

    private:
        MountId mNextMountId;
        std::map<MountId, std::unique_ptr<Session>> mMountedFiles;
        std::unique_ptr<BS::thread_pool<>> mIoThreadPool;
        std::unique_ptr<BS::thread_pool<>> mProcessingThreadPool;
        std::unique_ptr<LRUCache> mCache;
    };

} // namespace motioncam
