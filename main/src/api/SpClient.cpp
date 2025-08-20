#include "api/SpClient.h"

#include <iomanip>
#include <memory>
#include <sstream>

// Library includes
#include "bell/http/Client.h"
#include "tao/json.hpp"

#include "api/CredentialsResolver.h"
#include "proto/MetadataPb.h"

using namespace cspot;

namespace {
class DefaultSpClient : public SpClient {
 public:
  DefaultSpClient(std::shared_ptr<bell::HTTPClient> httpClient,
                  std::shared_ptr<CredentialsResolver> credentialsResolver)
      : httpClient(std::move(httpClient)),
        credentialsResolver(std::move(credentialsResolver)) {}

  bell::Result<> putConnectState(cspot_proto::PutStateRequest& stateRequest,
                                 const std::string& deviceId) override;
  bell::Result<bell::HTTPResponse> contextResolve(
      const std::string& contextUri) override;
  bell::Result<bell::HTTPResponse> contextAutoplayResolve(
      cspot_proto::AutoplayContextRequest& request) override;
  bell::Result<bell::HTTPResponse> rawRequest(const std::string& uri) override;

  bell::Result<cspot_proto::Track> trackMetadata(
      const SpotifyId& trackId) override;

  bell::Result<cspot_proto::Episode> episodeMetadata(
      const SpotifyId& episodeId) override;

  bell::Result<std::string> resolveStorageInteractive(
      const std::vector<std::byte>& fileId, bool prefetch = false) override;

 private:
  const char* LOG_TAG = "SpClient";

  std::shared_ptr<bell::HTTPClient> httpClient;
  std::shared_ptr<CredentialsResolver> credentialsResolver;

  // Cached credentials from credentials resolver
  std::string spClientAddress;
  std::string accessToken;
  std::string clientToken;

  // Fetches all required credentials from the credentials resolver
  bell::Result<> updateCredentials();
};

bell::Result<> DefaultSpClient::putConnectState(
    cspot_proto::PutStateRequest& stateRequest, const std::string& deviceId) {
  auto credentialsRes = updateCredentials();
  if (!credentialsRes) {
    // Could not fetch credentials
    return credentialsRes;
  }

  std::vector<std::byte> freshBuffer;
  auto encodeRes = nanopb_helper::encodeToVector(stateRequest, freshBuffer);

  if (!encodeRes) {
    BELL_LOG(error, LOG_TAG, "Error while encoding message");
    return bell::make_unexpected_errc(std::errc::bad_message);
  }

  uint32_t salt = std::rand();
  auto httpResponse = httpClient->put(
      fmt::format("https://{}/connect-state/v1/devices/{}?product=0&salt={}",
                  spClientAddress, deviceId, salt),
      {
          {
              "Content-Type",
              "application/x-protobuf",
          },
          // {"X-Spotify-Connection-Id", sessionContext->sessionInfo->sessionId},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      },
      tcb::span(reinterpret_cast<std::byte*>(freshBuffer.data()),
                freshBuffer.size()));

  if (!httpResponse) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             httpResponse.error());
    return tl::make_unexpected(httpResponse.error());
  }

  if (httpResponse->statusCode != 200) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             httpResponse->statusCode);
    return bell::make_unexpected_errc(std::errc::bad_message);
  }

  return {};
}

bell::Result<> DefaultSpClient::updateCredentials() {
  auto spClientAddressRes = credentialsResolver->getApAddress(
      CredentialsResolver::AddressType::SpClient);
  if (!spClientAddressRes) {
    return tl::make_unexpected(spClientAddressRes.error());
  }

  spClientAddress = *spClientAddressRes;

  auto accessTokenRes = credentialsResolver->getAccessKey();
  if (!accessTokenRes) {
    return tl::make_unexpected(accessTokenRes.error());
  }

  accessToken = *accessTokenRes;

  auto clientTokenRes = credentialsResolver->getClientToken();
  if (!clientTokenRes) {
    return tl::make_unexpected(clientTokenRes.error());
  }

  clientToken = *clientTokenRes;

  return {};
}

bell::Result<bell::HTTPResponse> DefaultSpClient::contextResolve(
    const std::string& contextUri) {
  return rawRequest(fmt::format("context-resolve/v1/{}", contextUri));
}

bell::Result<bell::HTTPResponse> DefaultSpClient::contextAutoplayResolve(
    cspot_proto::AutoplayContextRequest& request) {
  auto credentialsRes = updateCredentials();
  if (!credentialsRes) {
    // Could not fetch credentials
    return tl::make_unexpected(credentialsRes.error());
  }

  std::vector<std::byte> encodedBytes{};
  bool encodeResult = nanopb_helper::encodeToVector(request, encodedBytes);
  if (!encodeResult) {
    BELL_LOG(error, LOG_TAG, "Error while encoding AutoplayContextRequest");
    return bell::make_unexpected_errc<bell::HTTPResponse>(
        std::errc::bad_message);
  }

  return httpClient->post(
      fmt::format("https://{}/context-resolve/v1/autoplay", spClientAddress),
      {
          {"Client-Token", clientToken},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      },
      tcb::span(reinterpret_cast<std::byte*>(encodedBytes.data()),
                encodedBytes.size()));
}

bell::Result<bell::HTTPResponse> DefaultSpClient::rawRequest(
    const std::string& requestUri) {
  auto credentialsRes = updateCredentials();
  if (!credentialsRes) {
    // Could not fetch credentials
    return tl::make_unexpected(credentialsRes.error());
  }
  return httpClient->get(
      fmt::format("https://{}/{}", spClientAddress, requestUri),
      {
          {"Client-Token", clientToken},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      });
}

bell::Result<cspot_proto::Track> DefaultSpClient::trackMetadata(
    const SpotifyId& trackId) {
  if (trackId.type != SpotifyIdType::Track) {
    BELL_LOG(error, LOG_TAG, "Invalid track ID type: expected Track, got {}",
             static_cast<int>(trackId.type));
    return bell::make_unexpected_errc<cspot_proto::Track>(
        std::errc::invalid_argument);
  }

  auto response = httpClient->get(
      fmt::format("https://{}/metadata/4/track/{}", spClientAddress,
                  trackId.hexGid()),
      {
          {"Client-Token", clientToken},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      });

  if (!response) {
    return tl::make_unexpected(response.error());
  }

  if (response->statusCode != 200) {
    BELL_LOG(error, LOG_TAG, "Error while fetching track metadata: {}",
             response->statusCode);
    return bell::make_unexpected_errc<cspot_proto::Track>(
        std::errc::bad_message);
  }

  auto resultBytes = response->bytes();

  cspot_proto::Track trackProto;

  bool decodeRes = nanopb_helper::decodeFromVector(trackProto, *resultBytes);

  if (!decodeRes) {
    BELL_LOG(error, LOG_TAG, "Error while decoding track metadata");
    return bell::make_unexpected_errc<cspot_proto::Track>(
        std::errc::bad_message);
  }

  return trackProto;
}

bell::Result<cspot_proto::Episode> DefaultSpClient::episodeMetadata(
    const SpotifyId& episodeId) {
  if (episodeId.type != SpotifyIdType::Episode) {
    BELL_LOG(error, LOG_TAG, "Invalid track ID type: expected Episode, got {}",
             static_cast<int>(episodeId.type));
    return bell::make_unexpected_errc<cspot_proto::Episode>(
        std::errc::invalid_argument);
  }

  auto response = httpClient->get(
      fmt::format("https://{}/metadata/4/episode/{}", spClientAddress,
                  episodeId.hexGid()),
      {
          {"Client-Token", clientToken},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      });

  if (!response) {
    return tl::make_unexpected(response.error());
  }

  if (response->statusCode != 200) {
    BELL_LOG(error, LOG_TAG, "Error while fetching episode metadata: {}",
             response->statusCode);
    return bell::make_unexpected_errc<cspot_proto::Episode>(
        std::errc::bad_message);
  }

  auto resultBytes = response->bytes();

  cspot_proto::Episode episodeProto;

  bool decodeRes = nanopb_helper::decodeFromVector(episodeProto, *resultBytes);

  if (!decodeRes) {
    BELL_LOG(error, LOG_TAG, "Error while decoding episode metadata");
    return bell::make_unexpected_errc<cspot_proto::Episode>(
        std::errc::bad_message);
  }

  return episodeProto;
}

bell::Result<std::string> DefaultSpClient::resolveStorageInteractive(
    const std::vector<std::byte>& fileId, bool prefetch) {
  auto credentialsRes = updateCredentials();
  if (!credentialsRes) {
    // Could not fetch credentials
    return tl::make_unexpected(credentialsRes.error());
  }

  std::stringstream ss;
  ss << std::hex << std::setfill('0');  // Set hex output and pad with '0'

  for (const auto& byte : fileId) {
    ss << std::setw(2)
       << static_cast<unsigned>(byte);  // Convert byte to int for stream output
  }

  // Construct the endpoint URL depending on prefetch flag
  std::string endpoint =
      prefetch
          ? fmt::format(
                "https://{}/storage-resolve/files/audio/interactive_prefetch/"
                "{}?alt=json&product=9",
                spClientAddress, ss.str())
          : fmt::format(
                "https://{}/storage-resolve/files/audio/interactive/"
                "{}?alt=json&product=9",
                spClientAddress, ss.str());

  auto response = httpClient->get(
      endpoint, {
                    {"Client-Token", clientToken},
                    {"Authorization", fmt::format("Bearer {}", accessToken)},
                });

  if (!response) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             response.error());
    return tl::make_unexpected(response.error());
  }

  auto responseBody = *response->text();

  BELL_LOG(info, LOG_TAG, "Response body: {}", responseBody);
  tao::json::value obj = tao::json::from_string(responseBody);

  if (obj.at("cdnurl").is_array()) {
    return obj.at("cdnurl").get_array().at(0).get_string();
  }

  return bell::make_unexpected_errc<std::string>(std::errc::bad_message);
}
}  // namespace

std::unique_ptr<SpClient> cspot::createDefaultSpClient(
    std::shared_ptr<bell::HTTPClient> httpClient,
    std::shared_ptr<CredentialsResolver> credentialsResolver) {
  return std::make_unique<DefaultSpClient>(std::move(httpClient),
                                           std::move(credentialsResolver));
}
