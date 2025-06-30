#pragma once

#include <IVirtualFileSystem.h>
#include <BS_thread_pool.hpp>

namespace motioncam {

    class Decoder;
    class LRUCache;

    class VirtualFileSystemImpl_MCRAW : public IVirtualFileSystem
    {
    public:
        VirtualFileSystemImpl_MCRAW(
            BS::thread_pool<BS::tp::none>& ioThreadPool,
            BS::thread_pool<BS::tp::none>& processingThreadPool,
            LRUCache& lruCache,
            FileRenderOptions options,
            int draftScale,
            const std::string& file);

        ~VirtualFileSystemImpl_MCRAW();

        std::vector<Entry> listFiles(const std::string& filter = "") const override;
        std::optional<Entry> findEntry(const std::string& fullPath) const override;

        int readFile(
            const Entry& entry,
            const size_t pos,
            const size_t len,
            void* dst,
            std::function<void(size_t, int)> result,
            bool async = true) override;

        void updateOptions(FileRenderOptions options, int draftScale) override;

    private:
        void init(FileRenderOptions options);

        size_t generateFrame(
            const Entry& entry,
            const size_t pos,
            const size_t len,
            void* dst,
            std::function<void(size_t, int)> result,
            bool async);

        size_t generateAudio(
            const Entry& entry,
            const size_t pos,
            const size_t len,
            void* dst,
            std::function<void(size_t, int)> result,
            bool async);

    private:
        LRUCache& mCache;
        BS::thread_pool<BS::tp::none>& mIoThreadPool;
        BS::thread_pool<BS::tp::none>& mProcessingThreadPool;
        const std::string mSrcPath;
        const std::string mBaseName;
        size_t mTypicalDngSize;
        std::vector<Entry> mFiles;
        std::vector<uint8_t> mAudioFile;
        int mDraftScale;
        FileRenderOptions mOptions;
        float mFps;
        std::mutex mMutex;
    };

} // namespace motioncam
