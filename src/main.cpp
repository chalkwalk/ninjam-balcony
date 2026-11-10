#include "Version.h"

#include <cstdio>

int main() {
  std::printf("%s %s\n", balcony::kBotName, balcony::version());
  return 0;
}
