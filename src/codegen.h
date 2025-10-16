#pragma once

#include "design.h"

#include <memory>
#include <vector>
#include <string>

using std::unique_ptr;
using std::vector;
using std::string;

/**
 * @brief Generate C++ code for simulating a combine.
 * This includes generating the header lines ({combine_name}.h) and the C++ source lines ({combine_name}.cpp).
 * @param design The VulDesign object containing the combine and related definitions.
 * @param combine The name of the combine to generate code for.
 * @param headerlines Output vector to hold the generated header lines.
 * @param cpplines Output vector to hold the generated C++ source lines.
 * @return A unique_ptr to a vector of strings containing any error messages encountered during generation.
 *        Returns nullptr on success (no errors).
 */
unique_ptr<vector<string>> codegenCombine(VulDesign &design, const string &combine, vector<string> &headerlines, vector<string> &cpplines);


/**
 * @brief Generate C++ code for simulating an entire design.
 * @param design The VulDesign object containing the entire design.
 * @param cpplines Output vector to hold the generated C++ source lines for simulation.
 * @return A unique_ptr to a vector of strings containing any error messages encountered during generation.
 *        Returns nullptr on success (no errors).
 */
unique_ptr<vector<string>> codegenSimulation(VulDesign &design, vector<string> &cpplines);

/**
 * @brief Generate C++ code for simulating an entire design.
 * This includes generating the header files and C++ source files for all combines in the design.
 * @param design The VulDesign object containing the entire design.
 * @param libdir The directory where the vulsim core lib is located.
 * @param outdir The directory where the generated output files will be placed.
 * @return A unique_ptr to a vector of strings containing any error messages encountered during generation.
 *        Returns nullptr on success (no errors).
 */
unique_ptr<vector<string>> codegenProject(VulDesign &design, const string &libdir, const string &outdir);


