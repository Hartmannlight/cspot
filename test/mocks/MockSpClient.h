#pragma once

#include <trompeloeil.hpp>

#include "api/SpClient.h"

// Mock for SpClient class
class MockSpClient : public cspot::SpClient {
 public:
  MAKE_MOCK2(putConnectState,
             bell::Result<>(cspot_proto::PutStateRequest&, const std::string&),
             override);

  MAKE_MOCK1(contextResolve,
             bell::Result<bell::HTTPResponse>(const std::string&), override);

  MAKE_MOCK1(
      contextAutoplayResolve,
      bell::Result<bell::HTTPResponse>(cspot_proto::AutoplayContextRequest&),
      override);

  MAKE_MOCK1(rawRequest, bell::Result<bell::HTTPResponse>(const std::string&),
             override);

  MAKE_MOCK1(trackMetadata,
             bell::Result<cspot_proto::Track>(const cspot::SpotifyId&),
             override);

  MAKE_MOCK1(episodeMetadata,
             bell::Result<cspot_proto::Episode>(const cspot::SpotifyId&),
             override);

  MAKE_MOCK2(resolveStorageInteractive,
             bell::Result<std::string>(const std::vector<std::byte>&, bool),
             override);
};
