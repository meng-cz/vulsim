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
#include "errormsg.hpp"

namespace serialize {

typedef struct {
    string  name;
    string  value;
    string  comment;
    string  group;
} ConfigItemRaw;

/**
 * @brief Parse config library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_configs Output parameter to hold the parsed ConfigItemRaw items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseConfigLibFromXMLFile(const string &filepath, vector<ConfigItemRaw> &out_configs);

/**
 * @brief Write config library to an XML file.
 * @param filepath The path to the XML file.
 * @param configs The ConfigItemRaw items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeConfigLibToXMLFile(const string &filepath, const vector<ConfigItemRaw> &configs);

typedef struct {
    string  name;
    string  type;
    string  uintlen;
    string  value;
    string  comment;
    vector<string> dims;
} BundleMemberRaw;

typedef struct {
    string  name;
    string  comment;
    bool    isenum;
    bool    isalias;
    vector<BundleMemberRaw> members;
} BundleItemRaw;

/**
 * @brief Parse bundle library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_bundles Output parameter to hold the parsed BundleItemRaw items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseBundleLibFromXMLFile(const string &filepath, vector<BundleItemRaw> &out_bundles);

/**
 * @brief Write bundle library to an XML file.
 * @param filepath The path to the XML file.
 * @param bundles The BundleItemRaw items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeBundleLibToXMLFile(const string &filepath, const vector<BundleItemRaw> &bundles);

} // namespace serialize
