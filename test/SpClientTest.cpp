#include <doctest/doctest.h>
#include <trompeloeil.hpp>

#include "api/CredentialsResolver.h"
#include "bell/http/Client.h"
#include "proto/ConnectPb.h"
#include "tao/json.hpp"

#include "api/SpClient.h"
#include "mocks/MockCredentialsResolver.h"
#include "mocks/MockHTTPTransport.h"

TEST_CASE("SpClientTests tests") {
  using trompeloeil::_;  // wild card for matching any value

  auto mockTransport = std::make_unique<MockHTTPTransport>();
  auto* rawMockTransport = mockTransport.get();
  auto mockCredentialsResolver = std::make_shared<MockCredentialsResolver>();
  auto* rawCredentialsResolver = mockCredentialsResolver.get();

  auto spClient = cspot::createDefaultSpClient(
      std::make_shared<bell::HTTPClient>(std::move(mockTransport)),
      mockCredentialsResolver);

  SUBCASE("putConnectState") {
    const std::string clientTokenMock = "clientTokenMock";
    const std::string accessKeyMock = "accessKeyMock";
    const std::string spAddressMock = "spclient.spotify.com";
    const std::string mockDeviceId = "deviceid123";

    SUBCASE(
        "should fetch credentials from credentials resolver and hit a correct "
        "endpoint") {

      // Mock the credentials resolver
      REQUIRE_CALL(*rawCredentialsResolver, getApAddress(_, _))
          .RETURN(spAddressMock);
      REQUIRE_CALL(*rawCredentialsResolver, getAccessKey(_))
          .RETURN(accessKeyMock);
      REQUIRE_CALL(*rawCredentialsResolver, getClientToken(_))
          .RETURN(clientTokenMock);

      REQUIRE_CALL(*rawMockTransport, execute(_))
          .WITH(_1.method == bell::HTTPMethod::PUT)
          .WITH(_1.uri.path == fmt::format("/connect-state/v1/devices/{}", mockDeviceId))
          .RETURN(createMockResponseString(200, "unused", "application/json"));
      cspot_proto::PutStateRequest putStateReq = {};
      CHECK(spClient->putConnectState(putStateReq, mockDeviceId));
    }
  }

  // TODO: Write some more tests for spclient
}
