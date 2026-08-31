#include "fixtures.h"
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace fixtures {

nlohmann::json load(const std::string& relPath) {
  const std::string path = std::string(SHITFIRE_FIXTURES_DIR) + "/" + relPath;
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("fixture not found: " + path);
  return nlohmann::json::parse(in);
}

std::uint64_t bits(double d) {
  std::uint64_t u = 0;
  std::memcpy(&u, &d, sizeof u);
  return u;
}

}
