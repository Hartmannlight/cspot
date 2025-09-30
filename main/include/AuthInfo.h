#pragma once

#include <string>
#include "Utils.h"
#include "authentication.pb.h"
#include "proto/AuthenticationPb.h"
#include "tao/json.hpp"

namespace {
const std::string deviceIdPrefix = "142137fd329622137a149016";
}

namespace cspot {
struct AuthInfo {
  AuthInfo() = default;
  AuthInfo(const std::string& deviceName) : deviceName(deviceName) {
    // Recalculate device ID
    this->deviceId = fmt::format("{}{:016x}", deviceIdPrefix,
                                 std::hash<std::string>{}(deviceName));
  }
  std::string deviceName;
  std::string deviceId;
  std::string sessionId;

  std::optional<cspot_proto::LoginCredentials> loginCredentials;

  inline std::string toJson() const {
    tao::json::value json;
    json["deviceName"] = deviceName;
    json["deviceId"] = deviceId;
    if (loginCredentials.has_value()) {
      json["username"] = loginCredentials->username;
      std::cout << "Encoding blob of size " << loginCredentials->authData.size()
                << std::endl;
      json["blob"] = base64Encode(loginCredentials->authData.data(),
                                  loginCredentials->authData.size());
      json["authType"] = static_cast<int>(loginCredentials->type);
    }
    return tao::json::to_string(json);
  }

  inline void assignDataFromJson(const std::string& jsonString) {
    auto json = tao::json::from_string(jsonString);
    deviceName = json.at("deviceName").get_string();
    deviceId = json.at("deviceId").get_string();
    auto username = json.optional<std::string>("username");
    auto blob = json.optional<std::string>("blob");
    auto authType = json.optional<int>("authType");
    if (username.has_value() && blob.has_value() && authType.has_value()) {
      cspot_proto::LoginCredentials credentials;
      credentials.username = *username;
      credentials.type = static_cast<AuthenticationType>(*authType);
      auto decodedBlob = base64Decode(*blob);
      credentials.authData.insert(credentials.authData.end(),
                                  decodedBlob.begin(), decodedBlob.end());
      loginCredentials = credentials;
    }
  }
};
}  // namespace cspot
