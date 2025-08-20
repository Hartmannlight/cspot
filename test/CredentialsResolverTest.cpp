#include <doctest/doctest.h>
#include <chrono>
#include <trompeloeil.hpp>

#include "Utils.h"
#include "api/CredentialsResolver.h"
#include "mocks/MockHTTPTransport.h"
#include "proto/ClientTokenPb.h"
#include "proto/NanoPBHelper.h"
#include "tao/json.hpp"

TEST_CASE("CredentialsResolver tests") {
  using trompeloeil::_;  // wild card for matching any value

  auto mockTransport = std::make_unique<MockHTTPTransport>();
  auto* rawMockTransport = mockTransport.get();

  auto mockAuthInfo = std::make_shared<cspot::AuthInfo>();
  auto resolver = cspot::createDefaultCredentialsResolver(
      std::make_shared<bell::HTTPClient>(std::move(mockTransport)),
      mockAuthInfo);

  SUBCASE("getApAddress") {
    const std::string url =
        "https://apresolve.spotify.com/"
        "?type=spclient&type=dealer-g2&type=accesspoint";

    const std::string ap1 = "ap1.spotify.com:4070";
    const std::string dealer1 = "dealer1.spotify.com:443";
    const std::string spclient1 = "spclient1.spotify.com:443";

    tao::json::value responseJSON = {
        {"accesspoint", tao::json::value::array({ap1})},
        {"dealer-g2", tao::json::value::array({dealer1})},
        {"spclient", tao::json::value::array({spclient1})}};
    const std::string responseBody = tao::json::to_string(responseJSON);

    SUBCASE("should fetch and cache addresses on the first call") {
      REQUIRE_CALL(*rawMockTransport, execute(_))
          .WITH(_1.method == bell::HTTPMethod::GET)
          .WITH(_1.uri.host == "apresolve.spotify.com")
          .RETURN(
              createMockResponseString(200, responseBody, "application/json"));

      // Call for the first type, this should trigger the fetch
      auto apRes = resolver->getApAddress(
          cspot::CredentialsResolver::AddressType::AccessPoint);
      CHECK(apRes.has_value());
      CHECK(*apRes == ap1);

      // Subsequent calls should hit the cache (no new HTTP call)
      auto dealerRes = resolver->getApAddress(
          cspot::CredentialsResolver::AddressType::Dealer);
      CHECK(dealerRes.has_value());
      CHECK(*dealerRes == dealer1);

      REQUIRE_CALL(*rawMockTransport, execute(_))
          .WITH(_1.method == bell::HTTPMethod::GET)
          .WITH(_1.uri.host == "apresolve.spotify.com")
          .RETURN(
              createMockResponseString(200, responseBody, "application/json"));

      // A request after a certain time should expire the address and fetch a new one
      dealerRes = resolver->getApAddress(
          cspot::CredentialsResolver::AddressType::Dealer,
          std::chrono::system_clock::now() + std::chrono::hours(2));
      CHECK(dealerRes.has_value());
      CHECK(*dealerRes == dealer1);
    }
  }

  SUBCASE("getClientToken") {
    const std::string mockedToken = "123456789";

    // Mock a response from server
    cspot_proto::ClientTokenResponse tokenResponse;
    tokenResponse.responseType =
        ClientTokenResponseType_RESPONSE_GRANTED_TOKEN_RESPONSE;
    tokenResponse.grantedToken.token = mockedToken;
    tokenResponse.grantedToken.expiresAfterSeconds = 360;
    tokenResponse.grantedToken.refreshAfterSeconds = 360;

    // Prepare response bytes
    std::vector<std::byte> responseBytes;
    CHECK(nanopb_helper::encodeToVector(tokenResponse, responseBytes));

    auto responseSpan = tcb::span<std::byte>(
        {reinterpret_cast<std::byte*>(responseBytes.data()),
         responseBytes.size()});

    SUBCASE("should fetch and cache the token on the first call") {
      REQUIRE_CALL(*rawMockTransport, execute(_))
          .WITH(_1.method == bell::HTTPMethod::POST)
          .WITH(_1.uri.host == "clienttoken.spotify.com")
          .RETURN(createMockResponseBytes(200, responseSpan,
                                          "application/x-protobuf"));

      // Call for the first time, this should trigger the fetch
      auto tokenRes = resolver->getClientToken();
      CHECK(tokenRes.has_value());
      CHECK(*tokenRes == mockedToken);

      // Second call, should use cached token
      tokenRes = resolver->getClientToken();
      CHECK(tokenRes.has_value());
      CHECK(*tokenRes == mockedToken);

      REQUIRE_CALL(*rawMockTransport, execute(_))
          .WITH(_1.method == bell::HTTPMethod::POST)
          .WITH(_1.uri.host == "clienttoken.spotify.com")
          .RETURN(createMockResponseBytes(200, responseSpan,
                                          "application/x-protobuf"));

      // Request made after a certain time should expire the request and make a new one
      tokenRes = resolver->getClientToken(std::chrono::system_clock::now() +
                                          std::chrono::seconds(360 * 2));
      CHECK(tokenRes.has_value());
      CHECK(*tokenRes == mockedToken);
    }
  }
}
