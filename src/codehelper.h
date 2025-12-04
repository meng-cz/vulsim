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

#pragma once

#include <string>
#include <vector>
#include <memory>

#include <inttypes.h>

using std::string;
using std::vector;
using std::unique_ptr;

namespace code_helper {

/**
 * @brief Extract the body of a member function from given lines of code.
 * Search for the class definition and then the member function definition.
 * @param lines The lines of code to search.
 * @param class_name The name of the class containing the function.
 * @param func_name The name of the member function to extract.
 * @return A unique_ptr to the function body string if found, nullptr otherwise.
 */
unique_ptr<vector<string>> extractFunctionBodyHpp(const vector<string> &lines, const string &class_name, const string &func_name);


/**
 * @brief Replace the body of a member function in given lines of code.
 * Search for the class definition and then the member function definition.
 * @param lines The lines of code to modify.
 * @param class_name The name of the class containing the function.
 * @param func_name The name of the member function whose body is to be replaced.
 * @param new_body The new body of the function as a vector of strings.
 * @return True if the replacement was successful, false otherwise.
 */
bool replaceFunctionBodyHpp(vector<string> &lines, const string &class_name, const string &func_name, const vector<string> &new_body);


/**
 * @brief Extract the body of a member function from given lines of code.
 * Search for the class_name::func_name definition.
 * @param lines The lines of code to search.
 * @param class_name The name of the class containing the function.
 * @param func_name The name of the member function to extract.
 * @return A unique_ptr to the function body string if found, nullptr otherwise.
 */
unique_ptr<vector<string>> extractFunctionBodyCpp(const vector<string> &lines, const string &class_name, const string &func_name);


/**
 * @brief Replace the body of a member function in given lines of code.
 * Search for the class_name::func_name definition.
 * @param lines The lines of code to modify.
 * @param class_name The name of the class containing the function.
 * @param func_name The name of the member function whose body is to be replaced.
 * @param new_body The new body of the function as a vector of strings.
 * @return True if the replacement was successful, false otherwise.
 */
bool replaceFunctionBodyCpp(vector<string> &lines, const string &class_name, const string &func_name, const vector<string> &new_body);


} // namespace code_helper
