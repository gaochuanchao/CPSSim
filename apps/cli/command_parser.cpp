/***
 * File: apps/cli/command_parser.cpp
 * Purpose: Implement the small dependency-free CPSSim command tokenizer.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Quoting affects terminal tokenization only and never simulation data.
 ***/

#include "apps/cli/command_parser.hpp"

#include <cctype>
#include <stdexcept>

namespace cpssim {

/*** Tokenizes one line with explicit quote and escape state. ***/
std::vector<std::string> parse_cli_command_line(std::string_view line) {
    enum class Quote { None, Single, Double };

    std::vector<std::string> tokens;
    std::string token;
    Quote quote = Quote::None;
    bool escaping = false;
    bool token_started = false;

    for (const char character : line) {
        if (escaping) {
            token.push_back(character);
            token_started = true;
            escaping = false;
            continue;
        }
        if (character == '\\' && quote != Quote::Single) {
            escaping = true;
            token_started = true;
            continue;
        }
        if (character == '\'' && quote != Quote::Double) {
            quote = quote == Quote::Single ? Quote::None : Quote::Single;
            token_started = true;
            continue;
        }
        if (character == '"' && quote != Quote::Single) {
            quote = quote == Quote::Double ? Quote::None : Quote::Double;
            token_started = true;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(character)) != 0 && quote == Quote::None) {
            if (token_started) {
                tokens.push_back(std::move(token));
                token.clear();
                token_started = false;
            }
            continue;
        }
        token.push_back(character);
        token_started = true;
    }

    if (escaping) {
        throw std::invalid_argument{"command line ends with an unfinished escape"};
    }
    if (quote != Quote::None) {
        throw std::invalid_argument{"command line contains an unterminated quote"};
    }
    if (token_started) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

} // namespace cpssim
