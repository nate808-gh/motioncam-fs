#include "VirtualFileSystemImpl_MCRAW.h"
#include "CameraFrameMetadata.h"
#include "CameraMetadata.h"
#include "Utils.h"
#include "AudioWriter.h"
#include "LRUCache.h"

#include <motioncam/Decoder.hpp>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include <BS_thread_pool.hpp>
#include <spdlog/spdlog.h>
#include <audiofile/AudioFile.h>

#include <algorithm>
#include <sstream>
#include <tuple>

namespace motioncam {

namespace {

#ifdef _WIN32
constexpr std::string_view DESKTOP_INI = R"([.ShellClassInfo]
ConfirmFileOp=0

[ViewState]
Mode=4
Vid={137E7700-3573-11CF-AE69-08002B2E1262}
FolderType=Generic

[{5984FFE0-28D4-11CF-AE66-08002B2E1262}]
Mode=4
LogicalViewMode=1
IconSize=16

[LocalizedFileNames]
)";
#endif

std::string extractFilenameWithoutExtension(const std::string& fullPath) {
    boost::filesystem::path p(fullPath);
    return p.stem().string();
}

float calculateFrameRate(const std::vector<Timestamp>& frames) {
    if (frames.size() < 2) return 0.0f;

    double avgDuration = 0.0;
    int validFrames = 0;

    for (size_t i = 1; i < frames.size(); ++i) {
        double duration = static_cast<double>(frames[i] - frames[i - 1]);
        if (duration > 0) {
            avgDuration += (duration - avgDuration) / (++validFrames);
        }
    }
    if (validFrames == 0) return 0.0f;
    return static_cast<float>(1e9 / avgDuration);
}

int64_t getFrameNumberFromTimestamp(Timestamp ts, Timestamp ref, float fps) {
    if (fps <= 0) return -1;
    int64_t diff = ts - ref;
    if (diff < 0) return -1;
    double nsPerFrame = 1e9 / fps;
    return static_cast<int64_t>(std::round(diff / nsPerFrame));
}

std::string constructFrameFilename(const std::string& baseName, int frame, int pad=6, const std::string& ext="") {
    std::ostringstream oss;
    oss << baseName << std::setfill('0') << std::setw(pad) << frame;
    if (!ext.empty()) {
        if (ext[0] != '.') oss << '.';
        oss << ext;
    }
    return oss.str();
}

void syncAudio(Timestamp videoTs, std::vector<AudioChunk>& chunks, int rate, int ch) {
    auto driftMs = (chunks[0].first - videoTs) * 1e-6f;
    if (std::abs(driftMs) > 1000) {
        spdlog::warn("Audio drift too large");
        return;
    }
    if (driftMs > 0) {
        int framesToRemove = static_cast<int>(std::round(driftMs * rate / 1000));
        int samplesToRemove = framesToRemove * ch;
        int removed = 0;
        auto it = chunks.begin();
        while (it != chunks.end() && removed < samplesToRemove) {
            int remain = samplesToRemove - removed;
            if (it->second.size() <= remain) {
                removed += it->second.size();
                it = chunks.erase(it);
            } else {
                it->second.erase(it->second.begin(), it->second.begin() + remain);
                it->first += static_cast<Timestamp>(remain * 1000 / rate);
                break;
            }
        }
    } else {
        auto silence = -driftMs;
        int frames = static_cast<int>(std::round(silence * rate / 1000));
        int samples = frames * ch;
        std::vector<int16_t> zeros(samples, 0);
        AudioChunk chunk = {videoTs, zeros};
        chunks.insert(chunks.begin(), chunk);
        for (auto it = chunks.begin() + 1; it != chunks.end(); ++it)
            it->first += silence;
    }
}

int getScaleFromOptions(FileRenderOptions opts, int draft) {
    return (opts & RENDER_OPT_DRAFT) ? draft : 1;
}

}

VirtualFileSystemImpl_MCRAW::VirtualFileSystemImpl_MCRAW(
    BS::thread_pool<>& ioThreadPool,
    BS::thread_pool<>& processingThreadPool,
    LRUCache& lru,
    FileRenderOptions opts,
    int draftScale,
    const std::string& file)
    : mCache(lru),
      mIoThreadPool(ioThreadPool),
      mProcessingThreadPool(processingThreadPool),
      mSrcPath(file),
      mBaseName(extractFilenameWithoutExtension(file)),
      mTypicalDngSize(0),
      mFps(0),
      mDraftScale(draftScale),
      mOptions(opts)
{
    init(opts);
}

VirtualFileSystemImpl_MCRAW::~VirtualFileSystemImpl_MCRAW() {
    spdlog::info("Destroying VirtualFileSystemImpl_MCRAW({})", mSrcPath);
}

void VirtualFileSystemImpl_MCRAW::init(FileRenderOptions opts) {
    Decoder decoder(mSrcPath);
    auto frames = decoder.getFrames();
    std::sort(frames.begin(), frames.end());
    if (frames.empty()) return;

    spdlog::debug("init with options {}", optionsToString(opts));

    mFiles.clear();
    mFps = calculateFrameRate(frames);

    std::vector<uint8_t> data;
    nlohmann::json meta;
    decoder.loadFrame(frames[0], data, meta);

    auto camConfig = CameraConfiguration::parse(decoder.getContainerMetadata());
    auto frameMeta = CameraFrameMetadata::parse(meta);

    auto dng = utils::generateDng(
        data, frameMeta, camConfig, mFps, 0, opts, getScaleFromOptions(opts, mDraftScale));

    mTypicalDngSize = dng->size();

    int lastPts = 0;
    mFiles.reserve(frames.size() * 2);

#ifdef _WIN32
    Entry ini;
    ini.type = FILE_ENTRY;
    ini.size = DESKTOP_INI.size();
    ini.name = "desktop.ini";
    mFiles.emplace_back(ini);
#endif

    Entry audio;
    std::vector<AudioChunk> chunks;
    decoder.loadAudio(chunks);
    if (!chunks.empty()) {
        auto fpsFrac = utils::toFraction(mFps);
        AudioWriter writer(mAudioFile, decoder.numAudioChannels(), decoder.audioSampleRateHz(), fpsFrac.first, fpsFrac.second);
        syncAudio(frames[0], chunks, decoder.audioSampleRateHz(), decoder.numAudioChannels());
        for (auto& x : chunks)
            writer.write(x.second, x.second.size() / decoder.numAudioChannels());
    }
    if (!mAudioFile.empty()) {
        audio.type = FILE_ENTRY;
        audio.size = mAudioFile.size();
        audio.name = "audio.wav";
        mFiles.emplace_back(audio);
    }

    for (auto& x : frames) {
        int pts = getFrameNumberFromTimestamp(x, frames[0], mFps);
        while (lastPts < pts) {
            Entry e;
            e.type = FILE_ENTRY;
            e.size = mTypicalDngSize;
            e.name = constructFrameFilename("frame-", lastPts, 6, "dng");
            e.userData = x;
            mFiles.emplace_back(e);
            ++lastPts;
        }
    }
}

std::vector<Entry> VirtualFileSystemImpl_MCRAW::listFiles(const std::string&) const {
    return mFiles;
}

std::optional<Entry> VirtualFileSystemImpl_MCRAW::findEntry(const std::string& full) const {
    for (const auto& e : mFiles) {
        if (boost::filesystem::path(full).relative_path() == e.getFullPath())
            return e;
    }
    return {};
}

size_t VirtualFileSystemImpl_MCRAW::generateFrame(
    const Entry& entry, const size_t pos, const size_t len,
    void* dst, std::function<void(size_t, int)> result, bool async)
{
    using FrameData = std::tuple<size_t, CameraConfiguration, CameraFrameMetadata, std::shared_ptr<std::vector<uint8_t>>>;

    auto cacheEntry = mCache.get(entry);
    if (cacheEntry && pos < cacheEntry->size()) {
        const size_t actual = std::min(len, cacheEntry->size() - pos);
        std::memcpy(dst, cacheEntry->data() + pos, actual);
        mCache.put(entry, cacheEntry);
        return actual;
    }

    auto future = mIoThreadPool.submit_task([entry, &src = mSrcPath, &opt = mOptions]() -> FrameData {
        thread_local std::map<std::string, std::unique_ptr<Decoder>> decoders;
        auto ts = std::get<Timestamp>(entry.userData);

        if (decoders.find(src) == decoders.end())
            decoders[src] = std::make_unique<Decoder>(src);

        auto& d = decoders[src];
        auto data = std::make_shared<std::vector<uint8_t>>();
        nlohmann::json meta;

        auto frames = d->getFrames();
        auto it = std::find(frames.begin(), frames.end(), ts);
        if (it == frames.end())
            throw std::runtime_error("frame not found");

        d->loadFrame(ts, *data, meta);
        size_t idx = std::distance(frames.begin(), it);

        return { idx, CameraConfiguration::parse(d->getContainerMetadata()), CameraFrameMetadata::parse(meta), data };
    });

    auto share = future.share();
    auto fps = mFps;
    auto draft = mDraftScale;

    auto task = [&opt = mOptions, &cache = mCache, entry, share, fps, draft, pos, len, dst, result]() {
        size_t readBytes = 0;
        int err = -1;
        try {
            auto decoded = share.get();
            auto [idx, cfg, meta, data] = std::move(decoded);
            auto dng = utils::generateDng(*data, meta, cfg, fps, idx, opt, getScaleFromOptions(opt, draft));
            if (dng && pos < dng->size()) {
                size_t actual = std::min(len, dng->size() - pos);
                std::memcpy(dst, dng->data() + pos, actual);
                readBytes = actual;
                err = 0;
            }
            cache.put(entry, dng);
        }
        catch (std::runtime_error& e) {
            spdlog::error("DNG generation failed: {}", e.what());
            cache.markLoadFailed(entry);
        }
        result(readBytes, err);
        return readBytes;
    };

    auto process = mProcessingThreadPool.submit_task(task);
    if (!async)
        return process.get();
    return 0;
}

size_t VirtualFileSystemImpl_MCRAW::generateAudio(
    const Entry& entry, const size_t pos, const size_t len,
    void* dst, std::function<void(size_t, int)>, bool)
{
    if (pos >= mAudioFile.size()) return 0;
    size_t actual = std::min(len, mAudioFile.size() - pos);
    std::memcpy(dst, mAudioFile.data() + pos, actual);
    return actual;
}

int VirtualFileSystemImpl_MCRAW::readFile(
    const Entry& entry, const size_t pos, const size_t len,
    void* dst, std::function<void(size_t, int)> result, bool async)
{
#ifdef _WIN32
    if (entry.name == "desktop.ini") {
        size_t actual = std::min(len, DESKTOP_INI.size() - pos);
        std::memcpy(dst, DESKTOP_INI.data() + pos, actual);
        return actual;
    }
#endif
    if (boost::ends_with(entry.name, "wav"))
        return generateAudio(entry, pos, len, dst, result, async);
    else if (boost::ends_with(entry.name, "dng"))
        return generateFrame(entry, pos, len, dst, result, async);
    return -1;
}

void VirtualFileSystemImpl_MCRAW::updateOptions(FileRenderOptions opts, int draft) {
    mDraftScale = draft;
    mOptions = opts;
    init(opts);
}

} // namespace motioncam
