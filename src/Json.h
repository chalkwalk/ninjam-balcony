#pragma once

#include <map>
#include <string>

// Just enough JSON for the wire to the page (DESIGN §9.3): writing strings,
// and reading the flat objects a page sends ({"t":"say","text":"..."}).
// Anything a page sends that is not a flat object of strings and numbers is
// refused, which is the point: the page has two things to say.
namespace balcony::json {

// A JSON string literal, quotes included.
std::string quote(const std::string &s);

// Parse a flat object; numbers come back as their text, `true`/`false`/`null`
// as those words. False on anything else (nesting, trailing junk, bad escapes).
bool parseFlat(const std::string &text, std::map<std::string, std::string> &out);

}  // namespace balcony::json
