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

#include "configlib.h"
#include "bundlelib.h"
#include "module.h"
#include "project.h"
#include "errormsg.hpp"

namespace serialize {

/**
 * @brief Parse config library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_configs Output parameter to hold the parsed VulConfigItem items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseConfigLibFromXMLFile(const string &filepath, vector<VulConfigItem> &out_configs);

/**
 * @brief Write config library to an XML file.
 * @param filepath The path to the XML file.
 * @param configs The VulConfigItem items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeConfigLibToXMLFile(const string &filepath, const vector<VulConfigItem> &configs);


/**
 * @brief Parse bundle library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_bundles Output parameter to hold the parsed VulBundleItem items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseBundleLibFromXMLFile(const string &filepath, vector<VulBundleItem> &out_bundles);

/**
 * @brief Write bundle library to an XML file.
 * @param filepath The path to the XML file.
 * @param bundles The VulBundleItem items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeBundleLibToXMLFile(const string &filepath, const vector<VulBundleItem> &bundles);


/**
 * @brief Parse a base module from an XML file.
 * @param filepath The path to the XML file.
 * @param out_module Output parameter to hold the parsed VulExternalModule.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseModuleBaseFromXMLFile(const string &filepath, VulExternalModule &out_module);

/**
 * @brief Write a base module to an XML file.
 * @param filepath The path to the XML file.
 * @param module The VulExternalModule to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeModuleBaseToXMLFile(const string &filepath, const VulExternalModule &module);

/**
 * @brief Parse a full module from an XML file.
 * @param filepath The path to the XML file.
 * @param out_module Output parameter to hold the parsed VulModule.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseModuleFromXMLFile(const string &filepath, VulModule &out_module);

/**
 * @brief Write a full module to an XML file.
 * @param filepath The path to the XML file.
 * @param module The VulModule to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeModuleToXMLFile(const string &filepath, const VulModule &module);


/**
 * @brief Parse a project from an XML file.
 * @param filepath The path to the XML file.
 * @param out_project Output parameter to hold the parsed VulProjectRaw.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseProjectFromXMLFile(const string &filepath, VulProjectRaw &out_project);

/**
 * @brief Write a project to an XML file.
 * @param filepath The path to the XML file.
 * @param project The VulProjectRaw to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeProjectToXMLFile(const string &filepath, const VulProjectRaw &project);

} // namespace serialize
