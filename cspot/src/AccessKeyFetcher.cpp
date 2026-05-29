#include "AccessKeyFetcher.h"

#include <cstring>           // for strrchr
#include <initializer_list>  // for initializer_list
#include <map>               // for operator!=, operator==
#include <type_traits>       // for remove_extent_t
#include <vector>            // for vector

#include "BellLogger.h"    // for AbstractLogger
#include "CSpotContext.h"  // for Context
#include "HTTPClient.h"
#include "Logger.h"            // for CSPOT_LOG
#include "MercurySession.h"    // for MercurySession, MercurySession::Res...
#include "TimeProvider.h"
#include "Utils.h"

#ifdef BELL_ONLY_CJSON
#include "cJSON.h"
#else
#include "nlohmann/json.hpp"      // for basic_json<>::object_t, basic_json
#include "nlohmann/json_fwd.hpp"  // for json
#endif


using namespace cspot;

// Client ID and secret are set via ctx->config.clientId / clientSecret
// (registered Spotify app — never hardcode here)

AccessKeyFetcher::AccessKeyFetcher(std::shared_ptr<cspot::Context> ctx)
    : ctx(ctx) {}

bool AccessKeyFetcher::isExpired() {
  if (accessKey.empty()) {
    return true;
  }

  if (ctx->timeProvider->getSyncedTimestamp() > expiresAt) {
    return true;
  }

  return false;
}

std::string AccessKeyFetcher::getAccessKey() {
  if (!isExpired()) {
    return accessKey;
  }

  updateAccessKey();

  return accessKey;
}

void AccessKeyFetcher::updateAccessKey() {
  if (keyPending) {
    return;
  }
  keyPending = true;

  std::string body = "grant_type=client_credentials"
                     "&client_id=" + ctx->config.clientId +
                     "&client_secret=" + ctx->config.clientSecret;

  CSPOT_LOG(info, "Fetching access token via client credentials...");

  auto response = bell::HTTPClient::post(
      "https://accounts.spotify.com/api/token",
      {{"Content-Type", "application/x-www-form-urlencoded"}},
      std::vector<uint8_t>(body.begin(), body.end()));

  auto result = response->body();

#ifdef BELL_ONLY_CJSON
  cJSON* json = cJSON_Parse(result.data());
  if (json) {
    cJSON* token = cJSON_GetObjectItem(json, "access_token");
    cJSON* expires = cJSON_GetObjectItem(json, "expires_in");
    if (token && token->valuestring) {
      accessKey = token->valuestring;
      int expiresIn = expires ? expires->valueint / 2 : 1800;
      this->expiresAt = ctx->timeProvider->getSyncedTimestamp() + (expiresIn * 1000LL);
      CSPOT_LOG(info, "Access token fetched successfully");
    } else {
      CSPOT_LOG(error, "No access_token in response");
    }
    cJSON_Delete(json);
  }
#else
  auto json = nlohmann::json::parse(result, nullptr, false);
  if (!json.is_discarded() && json.contains("access_token")) {
    accessKey = json["access_token"];
    int expiresIn = json.value("expires_in", 3600) / 2;
    this->expiresAt = ctx->timeProvider->getSyncedTimestamp() + (expiresIn * 1000LL);
    CSPOT_LOG(info, "Access token fetched successfully");
  } else {
    CSPOT_LOG(error, "Failed to fetch access token");
  }
#endif

  keyPending = false;
}
