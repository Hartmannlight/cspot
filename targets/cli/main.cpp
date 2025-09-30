#include <fstream>
#include <istream>

#include "AuthInfo.h"
#include "Authenticator.h"
#include "Session.h"
#include "Utils.h"
#include "audio/CDNDataStream.h"
#include "bell/Logger.h"
#include "bell/audio/Common.h"
#include "bell/audio/OggContainer.h"
#include "bell/http/Client.h"
#include "bell/http/Server.h"
#include "bell/io/FileDataStream.h"
#include "bell/mdns/Manager.h"
#include "bell/utils/Semaphore.h"
#include "proto/AuthenticationPb.h"

namespace {
const char* sessionFilePath = "session.json";
}

void waitForZeroconfAuth(std::shared_ptr<cspot::AuthInfo> authInfo) {
  auto httpServer = std::make_shared<bell::http::Server>();
  auto authenticator = std::make_unique<cspot::Authenticator>();
  bell::Semaphore authSemaphore;

  httpServer->registerGet(
      "/spotify_handler",
      [&](const std::unique_ptr<bell::http::Reader>& requestReader,
          const std::unique_ptr<bell::http::Writer>& responseWriter,
          const auto& routeParams) {
        auto queryParams = *requestReader->getQueryParams();
        BELL_LOG(info, "Zeroconf", "Received GET Request");

        if (queryParams.find("action") != queryParams.end() &&
            queryParams["action"] == "getInfo") {
          auto zeroConfString = authenticator->buildZeroconfJSONResponse(
              authInfo->deviceName, authInfo->deviceId, "");
          (void)responseWriter->writeResponseWithBody(
              200, {{"Content-Type", "application/json"}}, zeroConfString);
        } else {
          (void)responseWriter->writeResponseWithBody(500, {},
                                                      "Invalid action");
        }
      });

  httpServer->registerPost(
      "/spotify_handler",
      [&](const std::unique_ptr<bell::http::Reader>& requestReader,
          const std::unique_ptr<bell::http::Writer>& responseWriter,
          const auto& routeParams) {
        std::cout << "Received post request" << std::endl;
        auto bodyStr = *requestReader->getBodyStringView();
        tao::json::value responseJson;
        responseJson["status"] = 101;
        responseJson["statusString"] = "OK";
        responseJson["spotifyError"] = 0;

        auto responseString = tao::json::to_string(responseJson);
        (void)responseWriter->writeResponseWithBody(
            200, {{"Content-Type", "application/json"}}, responseString);

        auto res = authenticator->authenticateZeroconfString(authInfo->deviceId,
                                                             bodyStr);
        if (res) {
          BELL_LOG(info, "Zeroconf", "authenticated with spotify");
          authInfo->loginCredentials = *res;
          BELL_LOG(info, "Zeroconf", "user: {}, bloblen={}", res->username,
                   res->authData.size());
        } else {
          BELL_LOG(error, "Zeroconf", "failed to authenticate with spotify");
        }

        authSemaphore.give();
      });

  (void)httpServer->listen(2139);
  auto service =  // Register mdns service, for spotify to find us
      bell::mdns::getDefaultManager()->advertise(
          authInfo->deviceName, "_spotify-connect._tcp", "", "", 2139,
          {{"VERSION", "1.0"}, {"CPath", "/spotify_handler"}, {"Stack", "SP"}});

  authSemaphore.take();
};

void testCDNFile() {
  std::string audioKey = "";
  std::string cdnUrl = "";

  auto audioKeyBytes = cspot::base64Decode(audioKey);

  auto dataStream = std::make_shared<cspot::CDNDataStream>(
      std::make_shared<bell::HTTPClient>());
  auto res = dataStream->open(cdnUrl, audioKeyBytes);
  if (!res) {
    BELL_LOG(error, "Main", "Failed to open cdn data stream: {}", res.error());
    return;
  }
  std::ifstream f("test.ogg", std::ios::binary);
  if (!f.is_open()) {
    BELL_LOG(error, "Main", "Failed to open test.ogg");
    return;
  }

  // auto dataStream = std::make_shared<bell::io::FileDataStream>(std::move(f));

  auto oggContainer = std::make_shared<bell::audio::OggContainer>();
  auto oggRes = oggContainer->openForRead(dataStream);
  if (!oggRes) {
    BELL_LOG(error, "Main", "Failed to open Ogg container: {}", oggRes.error());
    return;
  }

  auto vorbisCodec = std::make_shared<bell::audio::TremorVorbisCodec>();
  (void)oggContainer->readNextPacket();

  for (int x = 0; x < 10; x++) {
    auto packet = oggContainer->readNextPacket();
    if (!packet) {
      BELL_LOG(error, "Main", "Failed to read Ogg packet: {}", packet.error());
      continue;
    }
    BELL_LOG(info, "Main", "Read Ogg packet with {} bytes",
             packet->data.size());
    auto vorbisRes = vorbisCodec->setupDecodeFromHeaders(packet->data);
    if (!vorbisRes) {
      BELL_LOG(error, "Main", "Failed to setup Vorbis decoder: {}",
               vorbisRes.error());
      return;
    }
    if (vorbisRes == bell::AudioCodec::SetupStatus::Ready) {
      BELL_LOG(info, "Main", "Setup ready");
      break;
    }
  }

  auto seekRes = oggContainer->seekToFrame(4500000, 10000);
  if (!seekRes) {
    BELL_LOG(error, "Main", "Failed to seek: {}", seekRes.error());
    return;
  } else {
    BELL_LOG(info, "Main", "Seeked to frame");
  }
  // while (true) {
  //   auto packet = oggContainer->readNextPacket();
  //   if (packet.error() == bell::audio::Errc::EndOfStream) {
  //     BELL_LOG(info, "Main", "Reached end of stream");
  //     break;
  //   }
  //   if (!packet) {
  //     BELL_LOG(error, "Main", "Failed to read Ogg packet: {}", packet.error());
  //     break;
  //   }
  //   BELL_LOG(info, "Main", "Read Ogg packet with {} bytes",
  //            packet->data.size());
  //   auto decodeRes = vorbisCodec->decode(packet->data);
  //   if (!decodeRes) {
  //     BELL_LOG(error, "Main", "Failed to decode Vorbis packet: {}",
  //              decodeRes.error());
  //     if (decodeRes.error() == bell::audio::Errc::EndOfStream) {
  //       BELL_LOG(info, "Main", "Reached end of stream");
  //       break;
  //     }
  //     continue;
  //   }
  //   BELL_LOG(info, "Main", "Decoded {} samples", decodeRes->pcm.size());
  // }
}

int main(int argc, char** argv) {
  bell::registerDefaultLogger();

  auto authInfo = std::make_shared<cspot::AuthInfo>("Cspot player");

  std::ifstream sessionFile(sessionFilePath, std::ios::binary);
  if (sessionFile.is_open()) {
    std::string sessionString((std::istreambuf_iterator<char>(sessionFile)),
                              std::istreambuf_iterator<char>());
    sessionFile.close();
    if (!sessionString.empty()) {
      authInfo->assignDataFromJson(sessionString);
    }
  } else {
    waitForZeroconfAuth(authInfo);
    if (!authInfo->loginCredentials.has_value() ||
        authInfo->loginCredentials->authData.empty()) {
      BELL_LOG(error, "Main", "No login credentials, exiting");
      return 1;
    }

    // Write session to file
    std::string sessionString = authInfo->toJson();
    std::ofstream outFile(sessionFilePath, std::ios::binary);
    if (outFile.is_open()) {
      outFile << sessionString;
      outFile.close();
    }
  }

  auto session = std::make_shared<cspot::Session>(authInfo);
  auto startRes = session->start();

  if (!startRes) {
    BELL_LOG(error, "Main", "Failed to start session: {}", startRes.error());
    return 1;
  }

  while (true) {
    session->runPoller();
  }
  return 0;
}
