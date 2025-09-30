#pragma once

// Protobufs
#include "api/ApClient.h"
#include "bell/Result.h"
#include "connect.pb.h"

#include "SessionContext.h"
#include "api/SpClient.h"
#include "tracks/TrackQueueHandler.h"

namespace cspot {

class ConnectStateHandler {
 public:
  ConnectStateHandler(std::shared_ptr<EventLoop> eventLoop,
                      std::shared_ptr<AuthInfo> authInfo,
                      std::shared_ptr<SpClient> spClient);

  bell::Result<> handlePlayerCommand(tao::json::value& messageJson);

  bell::Result<> putState(
      PutStateReason reason = PutStateReason_PLAYER_STATE_CHANGED);

 private:
  const char* LOG_TAG = "ConnectStateHandler";

  std::shared_ptr<EventLoop> eventLoop;
  std::shared_ptr<AuthInfo> authInfo;
  std::shared_ptr<SpClient> spClient;
  std::shared_ptr<TrackQueueHandler> trackQueueHandler;
  // std::shared_ptr<ApClient> apClient;

  // Holds the protobuf state
  cspot_proto::PutStateRequest putStateRequestProto;

  void initialize();

  bell::Result<> handleTransferCommand(std::string_view payloadDataStr,
                                       const tao::json::value& options);

  bell::Result<> handlePlayCommand(const tao::json::value& options);

  bell::Result<> handleSkipNextCommand();

  bell::Result<> handleSkipPrevCommand();

  bool encodeProtoTracks(pb_ostream_t* stream, const pb_field_t* field,
                         bool previous);

  static bool pbEncodeNextTracks(pb_ostream_t* stream, const pb_field_t* field,
                                 void* const* arg) {
    return static_cast<ConnectStateHandler*>(*arg)->encodeProtoTracks(
        stream, field, false);
  }

  static bool pbEncodePreviousTracks(pb_ostream_t* stream,
                                     const pb_field_t* field,
                                     void* const* arg) {
    return static_cast<ConnectStateHandler*>(*arg)->encodeProtoTracks(
        stream, field, true);
  }
};
}  // namespace cspot
