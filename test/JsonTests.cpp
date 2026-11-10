#include "Json.h"

#include <catch2/catch_test_macros.hpp>

using namespace balcony::json;

TEST_CASE("strings are quoted so a page reads them back exactly", "[json]") {
  CHECK(quote("plain") == "\"plain\"");
  CHECK(quote("say \"hi\"\\\n") == "\"say \\\"hi\\\"\\\\\\n\"");
  CHECK(quote(std::string("\x01", 1)) == "\"\\u0001\"");
  CHECK(quote("caf\xc3\xa9") == "\"caf\xc3\xa9\"");
}

TEST_CASE("a page's flat objects parse; anything else is refused", "[json]") {
  std::map<std::string, std::string> m;
  REQUIRE(parseFlat(R"({"t":"hello","name":"Alice"})", m));
  CHECK(m["t"] == "hello");
  CHECK(m["name"] == "Alice");
  REQUIRE(parseFlat(R"( { "t" : "say" , "text" : "caf\u00e9 \"x\" \ud83c\udfb8", "n": 3 } )", m));
  CHECK(m["text"] == "caf\xc3\xa9 \"x\" \xf0\x9f\x8e\xb8");
  CHECK(m["n"] == "3");
  CHECK(!parseFlat(R"({"t":{"nested":1}})", m));
  CHECK(!parseFlat(R"({"t":"x"} trailing)", m));
  CHECK(!parseFlat(R"({"t":"bad \q escape"})", m));
  CHECK(!parseFlat(R"({"t":"lone \udc00"})", m));
  CHECK(!parseFlat("not json", m));
  CHECK(parseFlat("{}", m));
}
