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

#include "configuration.h"
#include "platform/env.hpp"

#include <memory>
#include <fstream>
#include <sstream>
#include <atomic>

using std::unique_ptr;
using std::make_unique;

unique_ptr<unordered_map<string, string>> global_config_map_ini = nullptr;
unique_ptr<unordered_map<string, string>> global_config_map_cli = nullptr;
std::atomic<bool> configuration_initialized = false;

void initConfiguration(const string &ini_path, const unordered_map<string, string> &overrides) {
	// reset/clear maps
	global_config_map_ini = make_unique<unordered_map<string, string>>();
	global_config_map_cli = make_unique<unordered_map<string, string>>(overrides);

	if (ini_path.empty()) {
		return;
	}

	std::ifstream ifs(ini_path);
	if (!ifs.is_open()) {
		return;
	}

	auto ltrim = [](string &s) {
		size_t i = 0;
		while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
		if (i) s.erase(0, i);
	};
	auto rtrim = [](string &s) {
		size_t i = s.size();
		while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) --i;
		if (i != s.size()) s.erase(i);
	};

	string line;
	while (std::getline(ifs, line)) {
		// remove possible CR on Windows-formatted files
		if (!line.empty() && line.back() == '\r') line.pop_back();

		string s = line;
		ltrim(s);
		rtrim(s);
		if (s.empty()) continue;

		// skip comments and section headers
		if (s[0] == ';' || s[0] == '#') continue;
		if (s.front() == '[' && s.back() == ']') continue;

		auto pos = s.find('=');
		if (pos == string::npos) continue; // malformed, skip

		string key = s.substr(0, pos);
		string value = s.substr(pos + 1);
		rtrim(key);
		ltrim(key);
		rtrim(value);
		ltrim(value);

		if (key.empty()) continue;

		(*global_config_map_ini)[key] = value;
	}

    configuration_initialized.store(true, std::memory_order_release);
}

optional<string> getConfigValue(const string &key) {
    if (!configuration_initialized.load(std::memory_order_acquire)) {
        initConfiguration("", {});
    }

    // first check CLI overrides
    auto iter_cli = global_config_map_cli->find(key);
    if (iter_cli != global_config_map_cli->end()) {
        return iter_cli->second;
    }

    // check environment variable
    auto env_value = getEnvVar(key);
    if (env_value.has_value()) {
        return env_value;
    }

    // then check INI file
    auto iter_ini = global_config_map_ini->find(key);
    if (iter_ini != global_config_map_ini->end()) {
        return iter_ini->second;
    }

    return std::nullopt;
}




