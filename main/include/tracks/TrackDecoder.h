#pragma once

#include <string>
#include <vector>
#include "audio/CDNDataStream.h"
#include "bell/audio/OggContainer.h"
#include "bell/audio/TremorVorbisCodec.h"

namespace cspot {
class TrackDecoder {
 public:
  TrackDecoder();

  bell::Result<> open(const std::string& cdnUrl,
                      const std::vector<std::byte>& decryptKey);

  bell::Result<bell::AudioCodec::DecodeResult> decodePacket();

  bool isOpen() const;

  void resetStream();

  bool isEndOfStream() const;

 private:
  const char* LOG_TAG = "TrackDecoder";

  bool isActiveStream = false;
  bool isEOF = false;
  std::shared_ptr<CDNDataStream> cdnDataStream;
  std::unique_ptr<bell::audio::OggContainer> oggContainer;
  std::unique_ptr<bell::audio::TremorVorbisCodec> vorbisCodec;
};
}  // namespace cspot
