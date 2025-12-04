// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "codehelper.h"
 
#include <cctype>
#include <unordered_set>
namespace code_helper {

class TokenParser {
public:
    TokenParser(const vector<string> &lines) : lines(lines) {}

    bool nextToken(string &out_token, size_t &out_line, size_t &out_col) {
        // advance to next non-space character across lines
        while (true) {
            if (current_line >= lines.size()) return false;
            const string &ln = lines[current_line];
            if (current_col >= ln.size()) {
                // move to next line
                ++current_line;
                current_col = 0;
                continue;
            }
            // skip spaces and tabs
            char c = ln[current_col];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++current_col;
                continue;
            }
            break;
        }

        if (current_line >= lines.size()) return false;

        const string &ln = lines[current_line];
        size_t len = ln.size();
        size_t start_line = current_line;
        size_t start_col = current_col;

        char c = ln[current_col];

        // skip comments
        if (c == '/' && current_col + 1 < len && ln[current_col + 1] == '/') {
            // single line comment
            current_line += 1;
            current_col = 0;
            return nextToken(out_token, out_line, out_col);
        } else if (c == '/' && current_col + 1 < len && ln[current_col + 1] == '*') {
            // multi-line comment
            current_col += 2;
            while (true) {
                if (current_line >= lines.size()) return false; // unclosed comment
                const string &cln = lines[current_line];
                while (current_col < cln.size()) {
                    if (cln[current_col] == '*' && current_col + 1 < cln.size() && cln[current_col + 1] == '/') {
                        // end of comment
                        current_col += 2;
                        return nextToken(out_token, out_line, out_col);
                    }
                    ++current_col;
                }
                // move to next line
                ++current_line;
                current_col = 0;
            }
        }

        // identifier
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = current_col + 1;
            while (j < len && (std::isalnum(static_cast<unsigned char>(ln[j])) || ln[j] == '_')) ++j;
            out_token = ln.substr(current_col, j - current_col);
            out_line = start_line; out_col = start_col;
            current_col = j;
            return true;
        }

        // number literal (simple handling: decimal, hex 0x, float with ., exponent)
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t j = current_col;
            // hex
            if (j + 1 < len && ln[j] == '0' && (ln[j+1] == 'x' || ln[j+1] == 'X')) {
                j += 2;
                while (j < len && std::isxdigit(static_cast<unsigned char>(ln[j]))) ++j;
            } else {
                // digits
                while (j < len && std::isdigit(static_cast<unsigned char>(ln[j]))) ++j;
                // fractional
                if (j < len && ln[j] == '.') {
                    ++j;
                    while (j < len && std::isdigit(static_cast<unsigned char>(ln[j]))) ++j;
                }
                // exponent
                if (j < len && (ln[j] == 'e' || ln[j] == 'E')) {
                    ++j;
                    if (j < len && (ln[j] == '+' || ln[j] == '-')) ++j;
                    while (j < len && std::isdigit(static_cast<unsigned char>(ln[j]))) ++j;
                }
            }
            // suffixes like u, l, f
            while (j < len && (std::isalpha(static_cast<unsigned char>(ln[j])))) ++j;
            out_token = ln.substr(current_col, j - current_col);
            out_line = start_line; out_col = start_col;
            current_col = j;
            return true;
        }

        // single char symbol
        out_token = string(1, c);
        out_line = start_line; out_col = start_col;
        ++current_col;
        return true;
    }

    bool moveToFunctionBodyHpp(const string &class_name, const string &func_name, size_t &out_start_line, size_t &out_start_col) {
        string token;
        size_t line, col;

        // search for function definition
        while (nextToken(token, line, col)) {
            // search for "class class_name"
            if (token == string("class")) {
                if (!nextToken(token, line, col)) break;
                if (token != class_name) continue;

                // found class definition, now search for function
                while (nextToken(token, line, col)) {
                    // look for "func_name(xxx) {xxx}"
                    if (token != func_name) continue;
                    // next token should be '('
                    if (!nextToken(token, line, col)) break;
                    if (token != "(") continue;
                    // skip function parameters
                    int paren_level = 1;
                    while (nextToken(token, line, col)) {
                        if (token == "(") ++paren_level;
                        else if (token == ")") --paren_level;
                        if (paren_level == 0) break;
                    }
                    if (paren_level != 0) break; // unbalanced parentheses
                    // now expect '{'
                    while (nextToken(token, line, col)) {
                        if (token == "{") {
                            out_start_line = line;
                            out_start_col = col;
                            return true;
                        }
                    }
                    break;
                }
            }
        }
        return false;
    }

    bool moveToFunctionBodyCpp(const string &class_name, const string &func_name, size_t &out_start_line, size_t &out_start_col) {
        string token;
        size_t line, col;

        // search for function definition
        while (nextToken(token, line, col)) {
            // look for "return_type class_name::func_name(xxx) {xxx}"
            // skip return type
            if (token == class_name) {
                // next two token should be ':'
                if (!nextToken(token, line, col)) break;
                if (token != ":") continue;
                if (!nextToken(token, line, col)) break;
                if (token != ":") continue;
                // next token should be func_name
                if (!nextToken(token, line, col)) break;
                if (token != func_name) continue;
                // next token should be '('
                if (!nextToken(token, line, col)) break;
                if (token != "(") continue;
                // skip function parameters
                int paren_level = 1;
                while (nextToken(token, line, col)) {
                    if (token == "(") ++paren_level;
                    else if (token == ")") --paren_level;
                    if (paren_level == 0) break;
                }
                if (paren_level != 0) break; // unbalanced parentheses
                // now expect '{'
                while (nextToken(token, line, col)) {
                    if (token == "{") {
                        out_start_line = line;
                        out_start_col = col;
                        return true;
                    }
                }
                break;
            }
        }
        return false;
    }

    bool moveToFunctionBodyCpp(const string &func_name, size_t &out_start_line, size_t &out_start_col) {
        string token;
        size_t line, col;

        // search for function definition
        while (nextToken(token, line, col)) {
            // look for "func_name(xxx) {xxx}"
            if (token != func_name) continue;
            // next token should be '('
            if (!nextToken(token, line, col)) break;
            if (token != "(") continue;
            // skip function parameters
            int paren_level = 1;
            while (nextToken(token, line, col)) {
                if (token == "(") ++paren_level;
                else if (token == ")") --paren_level;
                if (paren_level == 0) break;
            }
            if (paren_level != 0) break; // unbalanced parentheses
            // now expect '{'
            while (nextToken(token, line, col)) {
                if (token == "{") {
                    out_start_line = line;
                    out_start_col = col;
                    return true;
                }
            }
            break;
        }
        return false;
    }

    bool moveToFunctionBodyEnd(size_t &out_end_line, size_t &out_end_col) {
        string token;
        size_t line, col;

        int brace_count = 1;
        while (nextToken(token, line, col)) {
            if (token == "{") ++brace_count;
            else if (token == "}") --brace_count;
            if (brace_count == 0) {
                out_end_line = line;
                out_end_col = col;
                return true;
            }
        }
        return false;
    }

private:
    vector<string> lines;
    size_t current_line = 0;
    size_t current_col = 0;
};

/**
 * @brief Extract the body of a member function from given lines of code.
 * Search for the class definition and then the member function definition.
 * @param lines The lines of code to search.
 * @param class_name The name of the class containing the function.
 * @param func_name The name of the member function to extract.
 * @return A unique_ptr to the function body string if found, nullptr otherwise.
 */
unique_ptr<vector<string>> extractFunctionBodyHpp(const vector<string> &lines, const string &class_name, const string &func_name) {
    TokenParser parser(lines);
    string token;
    size_t line, col;

    size_t func_start_quote_line = lines.size() + 1;
    size_t func_start_quote_col = 0;
    size_t func_end_quote_line = lines.size() + 1;
    size_t func_end_quote_col = 0;

    // search for function definition
    if (!parser.moveToFunctionBodyHpp(class_name, func_name, func_start_quote_line, func_start_quote_col)) {
        return nullptr;
    }
    if (!parser.moveToFunctionBodyEnd(func_end_quote_line, func_end_quote_col)) {
        return nullptr;
    }

    // extract function body lines
    auto out_body = std::make_unique<vector<string>>();
    if (func_start_quote_line == func_end_quote_line) {
        string s = lines[func_start_quote_line].substr(func_start_quote_col + 1, func_end_quote_col - func_start_quote_col - 1);
        size_t startpos = s.find_first_not_of(" \t\r\n");
        size_t endpos = s.find_last_not_of(" \t\r\n");
        if (startpos != string::npos && endpos != string::npos && endpos >= startpos) {
            out_body->push_back(s.substr(startpos, endpos - startpos + 1) + string("\n"));
        }
        return out_body;
    }

    size_t cur_line = func_start_quote_line;
    if (func_start_quote_col + 2 < lines[func_start_quote_line].size()) {
        string s = lines[func_start_quote_line].substr(func_start_quote_col + 1);
        // insert if s is a code line (not just spaces)
        size_t startpos = s.find_first_not_of(" \t\r\n");
        if (startpos != string::npos) {
            out_body->push_back(s.substr(startpos));
        }
        cur_line += 1;
    }
    while (cur_line < func_end_quote_line) {
        out_body->push_back(lines[cur_line]);
        cur_line += 1;
    }
    if (func_end_quote_col) {
        out_body->push_back(lines[func_end_quote_line].substr(0, func_end_quote_col));
    }

    return out_body;
}


/**
 * @brief Replace the body of a member function in given lines of code.
 * Search for the class definition and then the member function definition.
 * @param lines The lines of code to modify.
 * @param class_name The name of the class containing the function.
 * @param func_name The name of the member function whose body is to be replaced.
 * @param new_body The new body of the function as a vector of strings.
 * @return True if the replacement was successful, false otherwise.
 */
bool replaceFunctionBodyHpp(vector<string> &lines, const string &class_name, const string &func_name, const vector<string> &new_body) {
    TokenParser parser(lines);
    string token;
    size_t line, col;

    size_t func_start_quote_line = lines.size() + 1;
    size_t func_start_quote_col = 0;
    size_t func_end_quote_line = lines.size() + 1;
    size_t func_end_quote_col = 0;

    // search for function definition
    if (!parser.moveToFunctionBodyHpp(class_name, func_name, func_start_quote_line, func_start_quote_col)) {
        return false;
    }
    if (!parser.moveToFunctionBodyEnd(func_end_quote_line, func_end_quote_col)) {
        return false;
    }

    // replace function body lines
    vector<string> new_lines;
    // lines before function body
    for (size_t i = 0; i < func_start_quote_line; ++i) {
        new_lines.push_back(lines[i]);
    }
    if (func_start_quote_col) {
        // lines before quote and quote itself should be preserved
        new_lines.push_back(lines[func_start_quote_line].substr(0, func_start_quote_col + 1) + string("\n"));
    }
    // new body
    for (const string &bline : new_body) {
        new_lines.push_back(bline);
    }
    // lines after function body
    new_lines.push_back(lines[func_end_quote_line].substr(func_end_quote_col));
    for (size_t i = func_end_quote_line + 1; i < lines.size(); ++i) {
        new_lines.push_back(lines[i]);
    }
    lines.swap(new_lines);
    return true;
}

/**
 * @brief Extract the body of a member function from given lines of code.
 * Search for the class_name::func_name definition.
 * @param lines The lines of code to search.
 * @param class_name The name of the class containing the function.
 * @param func_name The name of the member function to extract.
 * @return A unique_ptr to the function body string if found, nullptr otherwise.
 */
unique_ptr<vector<string>> extractFunctionBodyCpp(const vector<string> &lines, const string &class_name, const string &func_name) {
    TokenParser parser(lines);
    string token;
    size_t line, col;

    size_t func_start_quote_line = lines.size() + 1;
    size_t func_start_quote_col = 0;
    size_t func_end_quote_line = lines.size() + 1;
    size_t func_end_quote_col = 0;

    // search for function definition
    if (!parser.moveToFunctionBodyCpp(class_name, func_name, func_start_quote_line, func_start_quote_col)) {
        return nullptr;
    }
    if (!parser.moveToFunctionBodyEnd(func_end_quote_line, func_end_quote_col)) {
        return nullptr;
    }

    // extract function body lines
    auto out_body = std::make_unique<vector<string>>();
    if (func_start_quote_line == func_end_quote_line) {
        string s = lines[func_start_quote_line].substr(func_start_quote_col + 1, func_end_quote_col - func_start_quote_col - 1);
        size_t startpos = s.find_first_not_of(" \t\r\n");
        size_t endpos = s.find_last_not_of(" \t\r\n");
        if (startpos != string::npos && endpos != string::npos && endpos >= startpos) {
            out_body->push_back(s.substr(startpos, endpos - startpos + 1) + string("\n"));
        }
        return out_body;
    }

    size_t cur_line = func_start_quote_line;
    if (func_start_quote_col + 2 < lines[func_start_quote_line].size()) {
        string s = lines[func_start_quote_line].substr(func_start_quote_col + 1);
        // insert if s is a code line (not just spaces)
        size_t startpos = s.find_first_not_of(" \t\r\n");
        if (startpos != string::npos) {
            out_body->push_back(s.substr(startpos));
        }
        cur_line += 1;
    }
    while (cur_line < func_end_quote_line) {
        out_body->push_back(lines[cur_line]);
        cur_line += 1;
    }
    if (func_end_quote_col) {
        out_body->push_back(lines[func_end_quote_line].substr(0, func_end_quote_col));
    }

    return out_body;
}

/**
 * @brief Replace the body of a member function in given lines of code.
 * Search for the class_name::func_name definition.
 * @param lines The lines of code to modify.
 * @param class_name The name of the class containing the function.
 * @param func_name The name of the member function whose body is to be replaced.
 * @param new_body The new body of the function as a vector of strings.
 * @return True if the replacement was successful, false otherwise.
 */
bool replaceFunctionBodyCpp(vector<string> &lines, const string &class_name, const string &func_name, const vector<string> &new_body) {
    TokenParser parser(lines);
    string token;
    size_t line, col;

    size_t func_start_quote_line = lines.size() + 1;
    size_t func_start_quote_col = 0;
    size_t func_end_quote_line = lines.size() + 1;
    size_t func_end_quote_col = 0;

    // search for function definition
    if (!parser.moveToFunctionBodyCpp(class_name, func_name, func_start_quote_line, func_start_quote_col)) {
        return false;
    }
    if (!parser.moveToFunctionBodyEnd(func_end_quote_line, func_end_quote_col)) {
        return false;
    }

    // replace function body lines
    vector<string> new_lines;
    // lines before function body
    for (size_t i = 0; i < func_start_quote_line; ++i) {
        new_lines.push_back(lines[i]);
    }
    if (func_start_quote_col) {
        // lines before quote and quote itself should be preserved
        new_lines.push_back(lines[func_start_quote_line].substr(0, func_start_quote_col + 1) + string("\n"));
    }
    // new body
    for (const string &bline : new_body) {
        new_lines.push_back(bline);
    }
    // lines after function body
    new_lines.push_back(lines[func_end_quote_line].substr(func_end_quote_col));
    for (size_t i = func_end_quote_line + 1; i < lines.size(); ++i) {
        new_lines.push_back(lines[i]);
    }
    lines.swap(new_lines);
    return true;
}




} // namespace code_helper
