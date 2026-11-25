#include "miniredis/text.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace miniredis {

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::vector<std::string> splitWords(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> words;
    for (std::string word; in >> word;) {
        words.push_back(std::move(word));
    }
    return words;
}

}  // namespace miniredis
