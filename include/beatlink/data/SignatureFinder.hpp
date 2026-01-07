#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AnalysisTagFinder.hpp"
#include "AnalysisTagListener.hpp"
#include "BeatGridFinder.hpp"
#include "BeatGridListener.hpp"
#include "MetadataFinder.hpp"
#include "SignatureListener.hpp"
#include "TrackMetadataListener.hpp"
#include "TrackMetadataUpdate.hpp"
#include "WaveformDetail.hpp"
#include "beatlink/LifecycleParticipant.hpp"

namespace beatlink::data {

class SignatureFinder : public beatlink::LifecycleParticipant {
public:
    static SignatureFinder& getInstance();

    bool isRunning() const override { return running_.load(); }

    std::unordered_map<int, std::string> getSignatures();
    std::string getLatestSignatureFor(int player);
    std::string getLatestSignatureFor(const beatlink::DeviceUpdate& update);

    void addSignatureListener(const SignatureListenerPtr& listener);
    void removeSignatureListener(const SignatureListenerPtr& listener);
    std::vector<SignatureListenerPtr> getSignatureListeners() const;

    std::string computeTrackSignature(const std::string& title,
                                      const std::optional<SearchableItem>& artist,
                                      int duration,
                                      const WaveformDetail& waveformDetail,
                                      const BeatGrid& beatGrid);

    void start();
    void stop();

private:
    SignatureFinder();

    void clearSignature(int player);
    void checkIfSignatureReady(int player);
    void handleUpdate(int player);

    void deliverSignatureUpdate(int player, const std::string& signature);

    std::shared_ptr<WaveformDetail> waveformFromAnalysisTag(const std::shared_ptr<AnalysisTag>& tag);

    std::atomic<bool> running_{false};

    mutable std::mutex pendingMutex_;
    std::condition_variable pendingCv_;
    std::deque<int> pendingUpdates_;
    std::thread queueHandler_;

    mutable std::mutex signaturesMutex_;
    std::unordered_map<int, std::string> signatures_;

    mutable std::mutex waveformsMutex_;
    std::unordered_map<int, std::shared_ptr<WaveformDetail>> rgbWaveforms_;

    mutable std::mutex listenersMutex_;
    std::vector<SignatureListenerPtr> signatureListeners_;

    TrackMetadataListenerPtr metadataListener_;
    AnalysisTagListenerPtr rgbWaveformListener_;
    BeatGridListenerPtr beatGridListener_;
    beatlink::LifecycleListenerPtr lifecycleListener_;
};

} // namespace beatlink::data
