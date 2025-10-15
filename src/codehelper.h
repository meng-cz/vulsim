#pragma once

#include "type.h"

#include <vector>
#include <unordered_map>
#include <memory>

using std::vector;
using std::unique_ptr;
using std::unordered_map;

/**
 * @brief Generate C++ struct definition code for a VulBundle.
 * @param vb The VulBundle object to generate code for.
 * @return A unique_ptr to a vector of strings, each string is a line of C++ code.
 *         Returns nullptr if the bundle has no members.
 */
unique_ptr<vector<string>> codeGenerateBundleStruct(VulBundle &vb);

/**
 * @brief Generate C++ header file code bundle.h for multiple VulBundle definitions.
 * @param bundles A vector of VulBundle objects to generate code for.
 * @return A unique_ptr to a vector of strings, each string is a line of C++ code.
 */
unique_ptr<vector<string>> codeGenerateBundleHeaderFile(vector<VulBundle> &bundles);

/**
 * @brief Generate C++ header file code bundle.h for multiple VulBundle definitions.
 * @param bundles A map of VulBundle objects to generate code for, indexed by bundle name.
 * @return A unique_ptr to a vector of strings, each string is a line of C++ code.
 */
unique_ptr<vector<string>> codeGenerateBundleHeaderFile(unordered_map<string, VulBundle> &bundles);


/**
 * @brief Get helper code lines for a combine.
 * This is the local variable/function declarations and initializations needed for user to code a tick/init/service.
 * This includes code for configs, storages, and function signatures.
 * @param vc The VulCombine object to generate code for.
 * @return A unique_ptr to a vector of strings, each string is a line of C++ code.
 */
unique_ptr<vector<string>> codeGenerateHelperLines(VulCombine &vc);

/**
 * @brief Generate a C++ function signature from function name, arguments, and return values.
 * @param funcname The name of the function.
 * @param args The vector of VulArgument representing the function arguments.
 * @param rets The vector of VulArgument representing the function return values.
 * @return The generated C++ function signature as a string.
 */
string codeGenerateFunctionSignature(const string &funcname, const vector<VulArgument> &args, const vector<VulArgument> &rets);

/**
 * @brief Extract the body of a function from a C++ source file.
 * The function is identified by its name. The body includes all lines between the opening and closing braces.
 * @param filename The name of the C++ source file to read.
 * @param funcname The name of the function whose body to extract.
 * @param err Error message in case of failure.
 * @return A unique_ptr to a vector of strings, each string is a line of the function body.
 *         Returns nullptr on failure, with err set to an appropriate error message.
 */
unique_ptr<vector<string>> codeExtractFunctionBodyFromFile(const string &filename, const string &funcname, string &err);
