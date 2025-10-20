/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "type.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

using std::unordered_map;
using std::vector;
using std::string;
using std::unique_ptr;

typedef struct {
    string name;
    string comment;
    
    vector<VulPipePort> pipein;
    vector<VulPipePort> pipeout;

    vector<VulRequest> request;
    vector<VulService> service;

    vector<VulConfig> config;
} VulPrefabMetaData;

typedef unordered_map<string, VulPrefabMetaData> VulPrefabMetaDataMap;

/**
 * @brief Load all prefab metadata from XML files recursively in a directory.
 * @param dir The directory to scan for prefab XML files.
 * @param err Error message in case of failure.
 * @return A unique_ptr to the loaded VulPrefabMetaDataMap, or nullptr on failure.
 */
unique_ptr<VulPrefabMetaDataMap> loadPrefabsFromDirectory(const string &dir, string &err);
