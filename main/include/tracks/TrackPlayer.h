#pragma once

#include <unordered_map>

#include "FileProvider.h"
#include "api/ApClient.h"
#include "api/SpClient.h"
#include "audio/CDNDataStream.h"
#include "bell/utils/Semaphore.h"
#include "bell/utils/Task.h"
#include "events/EventModels.h"
#include "tracks/TrackDecoder.h"

namespace cspot {
class TrackPlayer : public bell::Task {
 public:
  TrackPlayer(std::shared_ptr<cspot::EventLoop> eventLoop,
              std::shared_ptr<cspot::SpClient> spClient,
              std::shared_ptr<cspot::ApClient> apClient);

  ~TrackPlayer() override;

 private:
  const char* LOG_TAG = "TrackPlayer";

  std::shared_ptr<cspot::EventLoop> eventLoop;
  std::shared_ptr<cspot::SpClient> spClient;
  std::shared_ptr<cspot::ApClient> apClient;
  std::unique_ptr<cspot::FileProvider> fileProvider;
  bell::Semaphore queueUpdateSemaphore;

  std::recursive_mutex playbackMutex;

  std::vector<SpotifyId> playbackQueue;
  std::unordered_map<SpotifyId, ProvidedFile> providedTracks;

  int currentTrackIndex = 0;
  bool flushRequested = false;
  std::unique_ptr<TrackDecoder> trackDecoder;

  void taskLoop() override;

  void registerHandlers();
  void handleQueueUpdate(const TrackQueueUpdate& queueUpdate);
  void handleFileProvided(const ProvidedFile& providedFile);
  bool isCurrentTrackReady();
  void handlePlayEvent(bool play);
  void handleFlushEvent();

  void announceState();
};
}  // namespace cspot
