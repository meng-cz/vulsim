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

#include "serialize.h"

#include "pugixml.hpp"

#include <memory>

using std::vector;
using std::string;
using std::unique_ptr;
using std::make_unique;
using std::tuple;
using std::make_tuple;

namespace serialize {

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

string base64Encode(const string &data) {
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(data.data());
    const size_t len = data.size();
    if (len == 0) return string();

    string out;
    // 每 3 字节 => 4 个 Base64 字符，向上取整
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= len) {
        unsigned int octet_a = bytes[i++];
        unsigned int octet_b = bytes[i++];
        unsigned int octet_c = bytes[i++];

        unsigned int triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out.push_back(base64_chars[(triple >> 18) & 0x3F]);
        out.push_back(base64_chars[(triple >> 12) & 0x3F]);
        out.push_back(base64_chars[(triple >> 6) & 0x3F]);
        out.push_back(base64_chars[triple & 0x3F]);
    }

    // 处理剩余的 1 或 2 字节
    const size_t rem = len - i;
    if (rem) {
        unsigned int octet_a = bytes[i++];
        unsigned int octet_b = (rem == 2) ? bytes[i++] : 0u;

        unsigned int triple = (octet_a << 16) | (octet_b << 8);

        out.push_back(base64_chars[(triple >> 18) & 0x3F]);
        out.push_back(base64_chars[(triple >> 12) & 0x3F]);
        if (rem == 2) {
            out.push_back(base64_chars[(triple >> 6) & 0x3F]);
            out.push_back('=');
        } else { // rem == 1
            out.push_back('=');
            out.push_back('=');
        }
    }

    return out;
}

string base64Encode(const vector<string> &linedata) {
    // 1) 拼接所有行，若行末无换行则补一个\n
    string combined;
    // 预估容量，尽量减少 realloc
    size_t reserveSize = 0;
    for (const auto &line : linedata) {
        reserveSize += line.size();
        if (line.empty() || line.back() != '\n') reserveSize += 1;
    }
    combined.reserve(reserveSize);

    for (const auto &line : linedata) {
        combined.append(line);
        if (line.empty() || line.back() != '\n') combined.push_back('\n');
    }

    // 2) Base64 编码（不插入换行符）
    return base64Encode(combined);
}

string base64DecodeString(const string &b64data) {
    if (b64data.empty()) return string();

    // 构建解码表。值域：0..63 有效；254 表示 '='；255 表示无效/忽略字符
    static unsigned char dtable[256];
    static bool inited = false;
    if (!inited) {
        for (int i = 0; i < 256; ++i) dtable[i] = 255;
        for (int i = 'A'; i <= 'Z'; ++i) dtable[i] = static_cast<unsigned char>(i - 'A');
        for (int i = 'a'; i <= 'z'; ++i) dtable[i] = static_cast<unsigned char>(26 + (i - 'a'));
        for (int i = '0'; i <= '9'; ++i) dtable[i] = static_cast<unsigned char>(52 + (i - '0'));
        dtable[static_cast<unsigned char>('+')] = 62;
        dtable[static_cast<unsigned char>('/')] = 63;
        dtable[static_cast<unsigned char>('=')] = 254;
        // 其余保持 255（无效/忽略）
        inited = true;
    }

    // 预估输出大小（粗略）：每 4 个字符 -> 3 字节
    size_t validCount = 0;
    for (unsigned char c : b64data) {
        unsigned char v = dtable[c];
        if (v != 255) ++validCount; // 包含 '=' 在内
    }
    string out;
    if (validCount >= 4) out.reserve((validCount / 4) * 3);

    unsigned char quartet[4];
    int qlen = 0;

    auto flushQuartet = [&](const unsigned char q[4]) {
        // q 中的 0..63 为数据，254 为 '='
        // 情况：
        // - 无 '='：输出 3 字节
        // - 一个 '='：输出 2 字节
        // - 两个 '='：输出 1 字节
        unsigned char q0 = q[0], q1 = q[1], q2 = q[2], q3 = q[3];
        if (q2 == 254) { // '==' 或 '=' 出现在第三位 -> 实际为 2 个 '='
            unsigned int triple = (static_cast<unsigned int>(q0) << 18) |
                                  (static_cast<unsigned int>(q1) << 12);
            out.push_back(static_cast<char>((triple >> 16) & 0xFF));
        } else if (q3 == 254) { // 一个 '='
            unsigned int triple = (static_cast<unsigned int>(q0) << 18) |
                                  (static_cast<unsigned int>(q1) << 12) |
                                  (static_cast<unsigned int>(q2) << 6);
            out.push_back(static_cast<char>((triple >> 16) & 0xFF));
            out.push_back(static_cast<char>((triple >> 8) & 0xFF));
        } else { // 无 '='
            unsigned int triple = (static_cast<unsigned int>(q0) << 18) |
                                  (static_cast<unsigned int>(q1) << 12) |
                                  (static_cast<unsigned int>(q2) << 6) |
                                  (static_cast<unsigned int>(q3));
            out.push_back(static_cast<char>((triple >> 16) & 0xFF));
            out.push_back(static_cast<char>((triple >> 8) & 0xFF));
            out.push_back(static_cast<char>(triple & 0xFF));
        }
    };

    for (unsigned char c : b64data) {
        unsigned char v = dtable[c];
        if (v == 255) {
            // 忽略空白或无效字符（此处简单跳过）
            // 可选：仅忽略空白字符 ' ', '\n', '\r', '\t'，其他遇到直接中断
            continue;
        }
        if (v == 254) {
            // 填满当前 quartet 后立刻结束解码
            quartet[qlen++] = 254;
            while (qlen < 4) quartet[qlen++] = 254;
            flushQuartet(quartet);
            qlen = 0;
            break;
        }
        quartet[qlen++] = v;
        if (qlen == 4) {
            flushQuartet(quartet);
            qlen = 0;
        }
    }

    // 如果还有未满 4 的残片，尝试按无填充进行解码（RFC 4648 允许无填充）
    if (qlen) {
        // 用 0 填充到 4 位，并按缺少的位数推导输出字节
        for (int i = qlen; i < 4; ++i) quartet[i] = 254; // 视作 '='
        flushQuartet(quartet);
    }

    return out;
}

unique_ptr<vector<string>> base64DecodeLines(const string &b64data) {
    string decoded = base64DecodeString(b64data);
    auto lines = make_unique<vector<string>>();
    lines->reserve(16);

    size_t start = 0;
    while (true) {
        size_t pos = decoded.find('\n', start);
        if (pos == string::npos) break;
        // 包含换行符
        lines->push_back(decoded.substr(start, pos - start + 1));
        start = pos + 1;
    }
    // 末尾残留（无换行），也要补一个换行符
    if (start < decoded.size()) {
        string last = decoded.substr(start);
        last.push_back('\n');
        lines->push_back(std::move(last));
    }

    return lines;
}

static const tuple<uint32_t, uint32_t, uint32_t> ConfigLibVersion = {1, 0, 0};

/*
ConfigLibVersion: 1.0

```
configlib
|-- version : 版本号
|-- [configitem]
    |-- name : 配置项名称
    |-- value : 配置项值（或表达式）
    |-- (comment) : 配置项注释（可选）
    |-- (group) : 配置项分组（可选）
```
*/

/**
 * @brief Parse config library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_configs Output parameter to hold the parsed ConfigItemRaw items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseConfigLibFromXMLFile(const string &filepath, vector<ConfigItemRaw> &out_configs) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to load XML file '") + filepath + "': " + result.description());
    }
    pugi::xml_node root = doc.child("configlib");
    if (!root) {
        return EStr(EItemXMLRootMissing, string("Missing root element 'configlib' in XML file '") + filepath + "'");
    }

    // version
    pugi::xml_node version_node = root.child("version");
    if (!version_node) {
        return EStr(EItemXMLRequestMissing, string("Missing 'version' element in XML file '") + filepath + "'");
    }
    string version_str = version_node.text().as_string();
    uint32_t major = 0, minor = 0, patch = 0;
    if (sscanf(version_str.c_str(), "%u.%u.%u", &major, &minor, &patch) < 2) {
        return EStr(EItemXMLRequestMissing, string("Invalid 'version' format in XML file '") + filepath + "': " + version_str);
    }
    if (make_tuple(major, minor, patch) != ConfigLibVersion) {
        return EStr(EItemXMLVersionMismatch, string("Version mismatch in XML file '") + filepath + "': " + version_str);
    }

    // find all configitem nodes
    for (pugi::xml_node item_node : root.children("configitem")) {
        ConfigItemRaw item;

        // name
        pugi::xml_node name_node = item_node.child("name");
        if (!name_node) {
            return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a configitem in XML file '") + filepath + "'");
        }
        item.name = name_node.text().as_string();

        // value
        pugi::xml_node value_node = item_node.child("value");
        if (!value_node) {
            return EStr(EItemXMLRequestMissing, string("Missing 'value' element in configitem '") + item.name + "' in XML file '" + filepath + "'");
        }
        item.value = value_node.text().as_string();

        // comment (optional)
        pugi::xml_node comment_node = item_node.child("comment");
        if (comment_node) {
            item.comment = comment_node.text().as_string();
        } else {
            item.comment = "";
        }

        // group (optional)
        pugi::xml_node group_node = item_node.child("group");
        if (group_node) {
            item.group = group_node.text().as_string();
        } else {
            item.group = "";
        }

        out_configs.push_back(std::move(item));
    }

    return "";
}


/**
 * @brief Write config library to an XML file.
 * @param filepath The path to the XML file.
 * @param configs The ConfigItemRaw items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeConfigLibToXMLFile(const string &filepath, const vector<ConfigItemRaw> &configs) {
    pugi::xml_document doc;

    pugi::xml_node root = doc.append_child("configlib");

    // version
    pugi::xml_node version_node = root.append_child("version");
    char version_buf[64];
    snprintf(version_buf, sizeof(version_buf), "%u.%u.%u",
             std::get<0>(ConfigLibVersion),
             std::get<1>(ConfigLibVersion),
             std::get<2>(ConfigLibVersion));
    version_node.append_child(pugi::node_pcdata).set_value(version_buf);

    // configitem nodes
    for (const auto &item : configs) {
        pugi::xml_node item_node = root.append_child("configitem");

        // name
        pugi::xml_node name_node = item_node.append_child("name");
        name_node.append_child(pugi::node_pcdata).set_value(item.name.c_str());

        // value
        pugi::xml_node value_node = item_node.append_child("value");
        value_node.append_child(pugi::node_pcdata).set_value(item.value.c_str());

        // comment (optional)
        if (!item.comment.empty()) {
            pugi::xml_node comment_node = item_node.append_child("comment");
            comment_node.append_child(pugi::node_pcdata).set_value(item.comment.c_str());
        }

        // group (optional)
        if (!item.group.empty()) {
            pugi::xml_node group_node = item_node.append_child("group");
            group_node.append_child(pugi::node_pcdata).set_value(item.group.c_str());
        }
    }

    // Save to file
    bool saveSucceeded = doc.save_file(filepath.c_str(), PUGIXML_TEXT("\t"), pugi::format_default | pugi::format_no_declaration);
    if (!saveSucceeded) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to save XML file '") + filepath + "'");
    }

    return "";
}

static const tuple<uint32_t, uint32_t, uint32_t> BundleLibVersion = {1, 0, 0};

/*
BundleLibVersion: 1.0

```
bundlelib
|-- version : 版本号
|-- [bundle]
    |-- name : Bundle 名称
    |-- (comment) : Bundle 描述
    |-- (isenum) : 是否为枚举类型（可选，默认为 false）
    |-- (isalias) : 是否为别名类型（可选，默认为 false）
    |-- [member]
        |-- name : 成员名称
        |-- (type) : 成员类型（可选，非枚举类型时为必须）
        |-- (uintlen) : 成员类型位宽（可选，针对整数类型）
        |-- (value) : 成员值（可选，枚举类型时为必须）
        |-- (comment) : 成员描述（可选）
        |-- [dims] : 成员数组维度（可选，重复时代表多维）
```
*/

/**
 * @brief Parse bundle library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_bundles Output parameter to hold the parsed BundleItemRaw items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseBundleLibFromXMLFile(const string &filepath, vector<BundleItemRaw> &out_bundles) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to load XML file '") + filepath + "': " + result.description());
    }
    pugi::xml_node root = doc.child("bundlelib");
    if (!root) {
        return EStr(EItemXMLRootMissing, string("Missing root element 'bundlelib' in XML file '") + filepath + "'");
    }
    
    // version
    pugi::xml_node version_node = root.child("version");
    if (!version_node) {
        return EStr(EItemXMLRequestMissing, string("Missing 'version' element in XML file '") + filepath + "'");
    }
    string version_str = version_node.text().as_string();
    uint32_t major = 0, minor = 0, patch = 0;
    if (sscanf(version_str.c_str(), "%u.%u.%u", &major, &minor, &patch) < 2) {
        return EStr(EItemXMLRequestMissing, string("Invalid 'version' format in XML file '") + filepath + "': " + version_str);
    }
    if (make_tuple(major, minor, patch) != BundleLibVersion) {
        return EStr(EItemXMLVersionMismatch, string("Version mismatch in XML file '") + filepath + "': " + version_str);
    }

    // find all bundle nodes
    for (pugi::xml_node bundle_node : root.children("bundle")) {
        BundleItemRaw bundle;

        // name
        pugi::xml_node name_node = bundle_node.child("name");
        if (!name_node) {
            return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a bundle in XML file '") + filepath + "'");
        }
        bundle.name = name_node.text().as_string();

        // comment (optional)
        pugi::xml_node comment_node = bundle_node.child("comment");
        if (comment_node) {
            bundle.comment = comment_node.text().as_string();
        } else {
            bundle.comment = "";
        }

        // isenum (optional)
        pugi::xml_node isenum_node = bundle_node.child("isenum");
        if (isenum_node) {
            bundle.isenum = true;
        } else {
            bundle.isenum = false;
        }

        // isalias (optional)
        pugi::xml_node isalias_node = bundle_node.child("isalias");
        if (isalias_node) {
            bundle.isalias = true;
        } else {
            bundle.isalias = false;
        }

        // members
        for (pugi::xml_node member_node : bundle_node.children("member")) {
            BundleMemberRaw member;

            // name
            pugi::xml_node member_name_node = member_node.child("name");
            if (!member_name_node) {
                return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a member of bundle '") + bundle.name + "' in XML file '" + filepath + "'");
            }
            member.name = member_name_node.text().as_string();

            // type (optional)
            pugi::xml_node type_node = member_node.child("type");
            if (type_node) {
                member.type = type_node.text().as_string();
            } else {
                member.type = "";
            }

            // uintlen (optional)
            pugi::xml_node uintlen_node = member_node.child("uintlen");
            if (uintlen_node) {
                member.uintlen = uintlen_node.text().as_string();
            } else {
                member.uintlen = "";
            }

            // value (optional)
            pugi::xml_node value_node = member_node.child("value");
            if (value_node) {
                member.value = value_node.text().as_string();
            } else {
                member.value = "";
            }
            // dims (optional)
            for (pugi::xml_node dim_node : member_node.children("dim")) {
                member.dims.push_back(dim_node.text().as_string());
            }

            bundle.members.push_back(member);

        }

        out_bundles.push_back(bundle);
    }

    return "";
}

/**
 * @brief Write bundle library to an XML file.
 * @param filepath The path to the XML file.
 * @param bundles The BundleItemRaw items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeBundleLibToXMLFile(const string &filepath, const vector<BundleItemRaw> &bundles) {
    pugi::xml_document doc;

    pugi::xml_node root = doc.append_child("bundlelib");

    // version
    pugi::xml_node version_node = root.append_child("version");
    char version_buf[64];
    snprintf(version_buf, sizeof(version_buf), "%u.%u.%u",
             std::get<0>(BundleLibVersion),
             std::get<1>(BundleLibVersion),
             std::get<2>(BundleLibVersion));
    version_node.append_child(pugi::node_pcdata).set_value(version_buf);

    // bundle nodes
    for (const auto &bundle : bundles) {
        pugi::xml_node bundle_node = root.append_child("bundle");

        // name
        pugi::xml_node name_node = bundle_node.append_child("name");
        name_node.append_child(pugi::node_pcdata).set_value(bundle.name.c_str());

        // comment (optional)
        if (!bundle.comment.empty()) {
            pugi::xml_node comment_node = bundle_node.append_child("comment");
            comment_node.append_child(pugi::node_pcdata).set_value(bundle.comment.c_str());
        }

        // isenum (optional)
        if (bundle.isenum) {
            bundle_node.append_child("isenum");
        }

        // isalias (optional)
        if (bundle.isalias) {
            bundle_node.append_child("isalias");
        }

        // member nodes
        for (const auto &member : bundle.members) {
            pugi::xml_node member_node = bundle_node.append_child("member");

            // name
            pugi::xml_node member_name_node = member_node.append_child("name");
            member_name_node.append_child(pugi::node_pcdata).set_value(member.name.c_str());

            // type (optional)
            if (!member.type.empty()) {
                pugi::xml_node type_node = member_node.append_child("type");
                type_node.append_child(pugi::node_pcdata).set_value(member.type.c_str());
            }

            // uintlen (optional)
            if (!member.uintlen.empty()) {
                pugi::xml_node uintlen_node = member_node.append_child("uintlen");
                uintlen_node.append_child(pugi::node_pcdata).set_value(member.uintlen.c_str());
            }

            // value (optional)
            if (!member.value.empty()) {
                pugi::xml_node value_node = member_node.append_child("value");
                value_node.append_child(pugi::node_pcdata).set_value(member.value.c_str());
            }

            // dims (optional)
            for (const auto &dim : member.dims) {
                pugi::xml_node dim_node = member_node.append_child("dim");
                dim_node.append_child(pugi::node_pcdata).set_value(dim.c_str());
            }
        }
    }
    // Save to file
    bool saveSucceeded = doc.save_file(filepath.c_str(), PUGIXML_TEXT("\t"), pugi::format_default | pugi::format_no_declaration);
    if (!saveSucceeded) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to save XML file '") + filepath + "'");
    }
    return "";
}



} // namespace serialize
