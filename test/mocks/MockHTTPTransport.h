#pragma once

#include <trompeloeil.hpp>
#include "bell/http/Client.h"  // Your actual client header
#include "bell/http/Common.h"
#include "bell/http/Writer.h"
#include "tcb/span.hpp"

// Create a mock class for the Transport interface using trompeloeil
class MockHTTPTransport : public bell::http::Transport {
 public:
  MAKE_MOCK1(execute,
             bell::Result<bell::http::Response>(const bell::http::Request&),
             override);
};

// Helper function to create a valid bell::http::Response for tests.
inline bell::http::Response createMockResponseBytes(
    int statusCode, tcb::span<const std::byte> body,
    const std::string& contentType) {
  auto ss = std::make_shared<std::stringstream>();

  // Write the mocked response body to the stringstream.
  bell::http::Writer writer(bell::http::Direction::Response, ss);
  (void)writer.writeResponse(statusCode, {{"Content-Type", contentType}},
                             body.size());

  (void)writer.writeBodyRaw(body.data(), body.size());

  // Construct reader object from same stringstream
  bell::http::Reader reader(bell::http::Direction::Response, ss);

  // Read headers to ensure they are parsed correctly.
  (void)reader.readHeaders();

  return {std::move(reader)};
}

inline bell::http::Response createMockResponseString(
    int statusCode, const std::string& body, const std::string& contentType) {
  return createMockResponseBytes(
      statusCode,
      tcb::span<const std::byte>(
          {reinterpret_cast<const std::byte*>(body.data()), body.size()}),
      contentType);
}
