#pragma once

#include <trompeloeil.hpp>

#include "api/CredentialsResolver.h"

// Mock for CredentialsResolver class
class MockCredentialsResolver : public cspot::CredentialsResolver {
 public:
  MAKE_MOCK2(getApAddress,
             bell::Result<std::string>(AddressType, sysclock_timepoint),
             override);

  MAKE_MOCK1(getClientToken, bell::Result<std::string>(sysclock_timepoint),
             override);

  MAKE_MOCK1(getAccessKey, bell::Result<std::string>(sysclock_timepoint),
             override);
};
