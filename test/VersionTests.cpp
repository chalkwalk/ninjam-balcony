#include "Version.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("the bot knows its own name", "[version]") {
  REQUIRE(std::string(balcony::kBotName) == "balcony");
}
