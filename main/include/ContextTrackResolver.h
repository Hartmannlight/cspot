#pragma once

#include <yajl_parse.h>
#include <string>
#include <tcb/span.hpp>
#include "api/SpClient.h"
#include "proto/ConnectPb.h"

namespace cspot {
class ContextTrackResolver {
 public:
  ContextTrackResolver(std::shared_ptr<SpClient> spClient,
                       uint32_t maxWindowSize = 33,
                       uint32_t trackUpdateThreshold = 8);

  /**
   * @brief Sets the context for the resolver
   */
  void updateContext(const std::string& rootContextUrl,
                     std::optional<std::string>& currentTrackUid,
                     std::optional<std::string>& currentTrackUri);

  bell::Result<> setShuffle(bool shuffle);

  bell::Result<cspot_proto::ContextTrack> getCurrentTrack();

  tcb::span<cspot_proto::ContextTrack> previousTracks();
  tcb::span<cspot_proto::ContextTrack> nextTracks();

  bell::Result<cspot_proto::ContextTrack> skipForward(
      const cspot_proto::ContextTrack& track);

  bell::Result<cspot_proto::ContextTrack> skipBackward(
      const cspot_proto::ContextTrack& track);

  bell::Result<cspot_proto::ContextTrack> next();
  bell::Result<cspot_proto::ContextTrack> previous();

  // Context tracks IDs or URIs can sometimes be missing or invalid
  struct TrackId {
    std::optional<std::string> uid = std::nullopt;
    std::optional<std::string> uri = std::nullopt;

    TrackId() = default;

    TrackId(const std::string& uid, const std::string& uri) {
      if (!uid.empty()) {
        this->uid = uid;
      }
      if (!uri.empty()) {
        this->uri = uri;
      }
    }

    bool operator==(const TrackId& other) const {
      if (uid.has_value() && other.uid.has_value()) {
        return uid.value() == other.uid.value();
      }

      if (uri.has_value() && other.uri.has_value()) {
        return uri.value() == other.uri.value();
      }

      return false;
    }

    bool operator==(const cspot_proto::ContextTrack& other) const {
      if (!other.uid.empty() && uid.has_value()) {
        return uid.value() == other.uid;
      }
      if (!other.uri.empty() && uri.has_value()) {
        return uri.value() == other.uri;
      }
      return false;
    }
  };

  struct PageWindow {
    uint32_t size = 0;
    uint32_t start = 0;
    std::vector<uint32_t> shuffleIndexes;
  };

  struct PageMetadata {
    std::optional<std::string> pageUrl;
    std::optional<std::string> nextPageUrl;
    uint32_t trackCount = 0;
    bool isValid = false;
  };

  struct PageParseState {
    // Enumeration of possible parsing levels
    enum Level {
      ExpectKey,
      InPagesArray,
      InPageObject,
      InTracksArray,
      InTrackObject,
      SkipValue
    };
    // Start with expecting a key
    Level level = ExpectKey;

    // Last key parsed, used to determine which field we are currently parsing
    std::string lastKey;

    // Current depth in the JSON structure
    int depth = 0;

    // Currently parsed page metadata
    ContextTrackResolver::PageMetadata parsedPageMetadata;
    cspot_proto::ContextTrack parsedTrack{};

    // Indexes
    uint32_t currentPageIndex = 0;
    uint32_t trackIndexInPage = 0;

    // Called when a new track is parsed
    std::function<void(uint32_t pageIndex, uint32_t trackIndex,
                       const cspot_proto::ContextTrack&)>
        onTrackResolved;

    // Called when a page metadata is parsed
    std::function<void(uint32_t pageIndex,
                       const ContextTrackResolver::PageMetadata&)>
        onPageMetadataResolved;

    // Holds static functions for the json parser
    yajl_callbacks yajlCallbacks;
  };

 private:
  const char* LOG_TAG = "ContextTrackResolver";

  std::shared_ptr<SpClient> spClient;

  // Root context URL, without "context://" prefix
  std::string rootContextUrl;

  // Config
  TrackId currentTrackId;
  uint32_t maxWindowSize;
  uint32_t trackUpdateThreshold;

  // State for streaming parsing of context pages
  PageParseState pageParseState;

  // Whether the resolver is currently in a sliding window mode
  bool isSlidingWindow = true;

  // Temporary storage for the current track window
  std::vector<cspot_proto::ContextTrack> currentTrackWindow;

  // Holds page metadata for resolved pages
  std::vector<PageMetadata> pageMetadata;

  // Holds window metadata for resolved pages
  std::vector<PageWindow> pageWindows;

  TrackId targetTrackId;

  // Index of current page in the metadata and window vectors
  uint32_t currentPageIndex = 0;

  bool isShuffle = false;

  std::vector<cspot_proto::ContextTrack> contextTrackQueue;
  cspot_proto::ContextIndex lowestWindowIndex{0, 0};
  cspot_proto::ContextIndex highestWindowIndex{0, 0};

  std::optional<cspot_proto::ContextTrack> currentTrack;

  void onPageMetadataParsed(uint32_t pageIndex,
                            const ContextTrackResolver::PageMetadata& metadata);

  void onTrackParsed(uint32_t pageIndex, uint32_t trackIndex,
                     const cspot_proto::ContextTrack& track);

  bell::Result<> ensureContextTracks();

  void setAdvanceWindow();
  void setRetreatWindow();

  void resetParseState();

  void updateTracksFromWindow();

  bell::Result<> shuffleIdsInPage(uint32_t pageIdx);

  std::optional<uint32_t> getCurrentTrackInQueueIndex();

  bell::Result<> resolveRootContext();
  bell::Result<> resolveContextPage(uint32_t pageIndex);
};
}  // namespace cspot
