#include <doctest/doctest.h>
#include <fstream>
#include <iostream>
#include <tao/json.hpp>
#include <trompeloeil.hpp>
#include "Utils.h"
#include "api/CredentialsResolver.h"
#include "authentication.pb.h"
#include "bell/http/Client.h"

// #include "mocks/MockSpClient.h"

TEST_CASE("ContextTrackProvider tests") {
  using trompeloeil::_;  // wild card for matching any value

  auto authInfo = std::make_shared<cspot::AuthInfo>();

  auto credentialsResolver = cspot::createDefaultCredentialsResolver(
      std::make_unique<bell::HTTPClient>(), authInfo);

  std::ifstream sessionFile("session.json", std::ios::binary);
  if (sessionFile.is_open()) {
    std::string sessionBlob((std::istreambuf_iterator<char>(sessionFile)),
                            std::istreambuf_iterator<char>());
    sessionFile.close();
    auto jsonData = tao::json::from_string(sessionBlob);

    std::string authBlob = jsonData["authBlob"].as<std::string>();
    std::string deviceId = jsonData["deviceId"].as<std::string>();
    authInfo->loginCredentials->authData = cspot::base64Decode(authBlob);
    authInfo->loginCredentials->username =
        jsonData["username"].as<std::string>();
    authInfo->deviceId = deviceId;
    authInfo->loginCredentials->type =
        AuthenticationType_AUTHENTICATION_STORED_SPOTIFY_CREDENTIALS;

    std::cout << "Access key" << std::endl;
    std::cout << *credentialsResolver->getAccessKey() << std::endl;
  }
}
