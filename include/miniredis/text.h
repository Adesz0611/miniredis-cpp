#pragma once

#include <string>
#include <vector>

namespace miniredis {

std::string toUpper(std::string s);

/// Splits on whitespace; consecutive separators are collapsed.
std::vector<std::string> splitWords(const std::string& line);

}  // namespace miniredis
