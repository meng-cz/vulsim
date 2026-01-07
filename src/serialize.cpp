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
 * @param out_configs Output parameter to hold the parsed VulConfigItem items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseConfigLibFromXMLFile(const string &filepath, vector<VulConfigItem> &out_configs) {
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
        VulConfigItem item;

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

        out_configs.push_back(std::move(item));
    }

    return "";
}


/**
 * @brief Write config library to an XML file.
 * @param filepath The path to the XML file.
 * @param configs The VulConfigItem items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeConfigLibToXMLFile(const string &filepath, const vector<VulConfigItem> &configs) {
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

ErrorMsg _parseBundleNode(pugi::xml_node &bundle_node, VulBundleItem &bundle, const string &filepath) {
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
    bool isenum = false;
    if (isenum_node) {
        isenum = true;
    }

    // isalias (optional)
    pugi::xml_node isalias_node = bundle_node.child("isalias");
    if (isalias_node) {
        bundle.is_alias = true;
    } else {
        bundle.is_alias = false;
    }

    // members
    if (isenum) {
        // 枚举类型
        for (pugi::xml_node enum_member_node : bundle_node.children("member")) {
            VulBundleEnumMember enum_member;

            // name
            pugi::xml_node enum_member_name_node = enum_member_node.child("name");
            if (!enum_member_name_node) {
                return EStr(EItemXMLRequestMissing, string("Missing 'name' element in an enum member of bundle '") + bundle.name + "' in XML file '" + filepath + "'");
            }
            enum_member.name = enum_member_name_node.text().as_string();

            // value
            pugi::xml_node enum_member_value_node = enum_member_node.child("value");
            if (!enum_member_value_node) {
                return EStr(EItemXMLRequestMissing, string("Missing 'value' element in enum member '") + enum_member.name + "' of bundle '" + bundle.name + "' in XML file '" + filepath + "'");
            }
            enum_member.value = enum_member_value_node.text().as_string();

            // comment (optional)
            pugi::xml_node enum_member_comment_node = enum_member_node.child("comment");
            if (enum_member_comment_node) {
                enum_member.comment = enum_member_comment_node.text().as_string();
            } else {
                enum_member.comment = "";
            }

            bundle.enum_members.push_back(enum_member);
        }
    } else {
        // 非枚举类型
        for (pugi::xml_node member_node : bundle_node.children("member")) {
            VulBundleMember member;

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
                member.uint_length = uintlen_node.text().as_string();
            } else {
                member.uint_length = "";
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
                string dim_str = dim_node.text().as_string();
                member.dims.push_back(dim_str);
            }

            bundle.members.push_back(member);
        }
    }
    return "";
}

/**
 * @brief Parse bundle library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_bundles Output parameter to hold the parsed BundleItemRaw items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseBundleLibFromXMLFile(const string &filepath, vector<VulBundleItem> &out_bundles) {
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
        VulBundleItem bundle;

        ErrorMsg err = _parseBundleNode(bundle_node, bundle, filepath);
        if (!err.empty()) {
            return err;
        }

        out_bundles.push_back(bundle);
    }

    return "";
}

ErrorMsg _writeBundleNode(pugi::xml_node &bundle_node, const VulBundleItem &bundle) {
    // name
    pugi::xml_node name_node = bundle_node.append_child("name");
    name_node.append_child(pugi::node_pcdata).set_value(bundle.name.c_str());

    // comment (optional)
    if (!bundle.comment.empty()) {
        pugi::xml_node comment_node = bundle_node.append_child("comment");
        comment_node.append_child(pugi::node_pcdata).set_value(bundle.comment.c_str());
    }

    // isenum (optional)
    if (bundle.enum_members.size() > 0) {
        bundle_node.append_child("isenum");
    }

    // isalias (optional)
    if (bundle.is_alias) {
        bundle_node.append_child("isalias");
    }

    // member nodes
    if (bundle.enum_members.size() == 0) {
        // 非枚举类型
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
            if (!member.uint_length.empty()) {
                pugi::xml_node uintlen_node = member_node.append_child("uintlen");
                uintlen_node.append_child(pugi::node_pcdata).set_value(member.uint_length.c_str());
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
    } else {
        // 枚举类型
        for (const auto &enum_member : bundle.enum_members) {
            pugi::xml_node enum_member_node = bundle_node.append_child("member");

            // name
            pugi::xml_node enum_member_name_node = enum_member_node.append_child("name");
            enum_member_name_node.append_child(pugi::node_pcdata).set_value(enum_member.name.c_str());

            // value
            pugi::xml_node enum_member_value_node = enum_member_node.append_child("value");
            enum_member_value_node.append_child(pugi::node_pcdata).set_value(enum_member.value.c_str());

            // comment (optional)
            if (!enum_member.comment.empty()) {
                pugi::xml_node enum_member_comment_node = enum_member_node.append_child("comment");
                enum_member_comment_node.append_child(pugi::node_pcdata).set_value(enum_member.comment.c_str());
            }
        }
    }
    return "";
}

/**
 * @brief Write bundle library to an XML file.
 * @param filepath The path to the XML file.
 * @param bundles The VulBundleItem items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeBundleLibToXMLFile(const string &filepath, const vector<VulBundleItem> &bundles) {
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
        ErrorMsg err = _writeBundleNode(bundle_node, bundle);
        if (!err.empty()) {
            return err;
        }
    }
    // Save to file
    bool saveSucceeded = doc.save_file(filepath.c_str(), PUGIXML_TEXT("\t"), pugi::format_default | pugi::format_no_declaration);
    if (!saveSucceeded) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to save XML file '") + filepath + "'");
    }
    return "";
}

static const tuple<uint32_t, uint32_t, uint32_t> MemberVersion = {1, 0, 0};


/*
ModuleVersion: 1.0

```
modulebase
|-- version : 版本号
|-- name : 模块名称
|-- (comment) : 模块描述（可选）
|-- [localconf]
    |-- name : 本地配置项名称
    |-- value : 本地配置项值（或表达式）
    |-- (comment) : 本地配置项注释（可选）
|-- [localbundle]
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
|-- [request/service]
    |-- name : 请求名称
    |-- (comment) : 请求描述（可选）
    |-- (array_size) : 请求数组大小（可选，非数组请求时为空）
    |-- (handshake) : 是否包含握手（可选，默认为 false）
    |-- [arg]
        |-- name : 参数名称
        |-- type : 参数类型
        |-- (comment) : 参数描述（可选）
    |-- [ret]
        |-- name : 返回值名称
        |-- type : 返回值类型
        |-- (comment) : 返回值描述（可选）
|-- [pipein/pipeout]
    |-- name : 端口名称
    |-- (comment) : 端口描述（可选）
    |-- type : 管道端口数据类型
```
*/

ErrorMsg _parseReqServNode(pugi::xml_node &node, VulReqServ &rs, const string &filepath) {
    pugi::xml_node rs_name = node.child("name");
    if (!rs_name) {
        return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a request/service in XML file '" + filepath + "'"));
    }
    rs.name = rs_name.text().as_string();

    if (pugi::xml_node rs_comment = node.child("comment")) rs.comment = rs_comment.text().as_string(); else rs.comment = "";
    rs.has_handshake = node.child("handshake");

    for (pugi::xml_node arg_node : node.children("arg")) {
        VulArg a;
        pugi::xml_node an = arg_node.child("name");
        if (!an) {
            return EStr(EItemXMLRequestMissing, string("Missing 'name' element in an arg of ") + rs.name + " in XML file '" + filepath + "'");
        }
        a.name = an.text().as_string();
        pugi::xml_node at = arg_node.child("type");
        if (!at) {
            return EStr(EItemXMLRequestMissing, string("Missing 'type' element in an arg '") + a.name + "' of " + rs.name + " in XML file '" + filepath + "'");
        }
        a.type = at.text().as_string();
        if (pugi::xml_node ac = arg_node.child("comment")) a.comment = ac.text().as_string(); else a.comment = "";
        rs.args.push_back(std::move(a));
    }

    for (pugi::xml_node ret_node : node.children("ret")) {
        VulArg r;
        pugi::xml_node rn = ret_node.child("name");
        if (!rn) {
            return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a ret of ") + rs.name + " in XML file '" + filepath + "'");
        }
        r.name = rn.text().as_string();
        pugi::xml_node rt = ret_node.child("type");
        if (!rt) {
            return EStr(EItemXMLRequestMissing, string("Missing 'type' element in a ret '") + r.name + "' of " + rs.name + " in XML file '" + filepath + "'");
        }
        r.type = rt.text().as_string();
        if (pugi::xml_node rc = ret_node.child("comment")) r.comment = rc.text().as_string(); else r.comment = "";
        rs.rets.push_back(std::move(r));
    }
    return "";
}

ErrorMsg _parseModuleBase(pugi::xml_node &root, VulModuleBase &out_module, const string &filepath) {

    // version
    pugi::xml_node version_node = root.child("version");
    if (!version_node) {
        return EStr(EItemXMLRequestMissing, string("Missing 'version' element in XML file '") + filepath + "'");
    }
    {
        string version_str = version_node.text().as_string();
        uint32_t major = 0, minor = 0, patch = 0;
        if (sscanf(version_str.c_str(), "%u.%u.%u", &major, &minor, &patch) < 2) {
            return EStr(EItemXMLRequestMissing, string("Invalid 'version' format in XML file '") + filepath + "': " + version_str);
        }
        if (make_tuple(major, minor, patch) != MemberVersion) {
            return EStr(EItemXMLVersionMismatch, string("Version mismatch in XML file '") + filepath + "': " + version_str);
        }
    }

    // name (required)
    pugi::xml_node name_node = root.child("name");
    if (!name_node) {
        return EStr(EItemXMLRequestMissing, string("Missing 'name' element in XML file '") + filepath + "'");
    }
    out_module.name = name_node.text().as_string();

    // comment (optional)
    if (pugi::xml_node comment_node = root.child("comment")) {
        out_module.comment = comment_node.text().as_string();
    } else {
        out_module.comment = "";
    }

    // localconf entries
    for (pugi::xml_node lc_node : root.children("localconf")) {
        VulLocalConfigItem lc;

        pugi::xml_node lc_name = lc_node.child("name");
        if (!lc_name) {
            return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a localconf in XML file '") + filepath + "'");
        }
        lc.name = lc_name.text().as_string();

        pugi::xml_node lc_value = lc_node.child("value");
        if (!lc_value) {
            return EStr(EItemXMLRequestMissing, string("Missing 'value' element in localconf '") + lc.name + "' in XML file '" + filepath + "'");
        }
        lc.value = lc_value.text().as_string();

        if (pugi::xml_node lc_comment = lc_node.child("comment")) {
            lc.comment = lc_comment.text().as_string();
        } else {
            lc.comment = "";
        }

        out_module.local_configs[lc.name] = std::move(lc);
    }

    // localbundle entries (structure similar to bundlelib)
    for (pugi::xml_node lb_node : root.children("localbundle")) {
        VulBundleItem bundle;
        ErrorMsg err = _parseBundleNode(lb_node, bundle, filepath);
        if (!err.empty()) {
            return err;
        }

        out_module.local_bundles[bundle.name] = std::move(bundle);
    }

    // requests
    for (pugi::xml_node req_node : root.children("request")) {
        VulReqServ req;
        ErrorMsg e = _parseReqServNode(req_node, req, filepath);
        if (!e.empty()) return e;
        out_module.requests[req.name] = std::move(req);
    }

    // services
    for (pugi::xml_node serv_node : root.children("service")) {
        VulReqServ serv;
        ErrorMsg e = _parseReqServNode(serv_node, serv, filepath);
        if (!e.empty()) return e;
        out_module.services[serv.name] = std::move(serv);
    }

    // pipein
    for (pugi::xml_node pin_node : root.children("pipein")) {
        VulPipePort p;
        pugi::xml_node pn = pin_node.child("name");
        if (!pn) {
            return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a pipein in XML file '") + filepath + "'");
        }
        p.name = pn.text().as_string();
        if (pugi::xml_node pc = pin_node.child("comment")) p.comment = pc.text().as_string(); else p.comment = "";
        pugi::xml_node pt = pin_node.child("type");
        if (!pt) {
            return EStr(EItemXMLRequestMissing, string("Missing 'type' element in pipein '") + p.name + "' in XML file '" + filepath + "'");
        }
        p.type = pt.text().as_string();
        out_module.pipe_inputs[p.name] = std::move(p);
    }

    // pipeout
    for (pugi::xml_node pout_node : root.children("pipeout")) {
        VulPipePort p;
        pugi::xml_node pn = pout_node.child("name");
        if (!pn) {
            return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a pipeout in XML file '") + filepath + "'");
        }
        p.name = pn.text().as_string();
        if (pugi::xml_node pc = pout_node.child("comment")) p.comment = pc.text().as_string(); else p.comment = "";
        pugi::xml_node pt = pout_node.child("type");
        if (!pt) {
            return EStr(EItemXMLRequestMissing, string("Missing 'type' element in pipeout '") + p.name + "' in XML file '" + filepath + "'");
        }
        p.type = pt.text().as_string();
        out_module.pipe_outputs[p.name] = std::move(p);
    }

    return "";
}

/**
 * @brief Parse a base module from an XML file.
 * @param filepath The path to the XML file.
 * @param out_module Output parameter to hold the parsed VulExternalModule.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseModuleBaseFromXMLFile(const string &filepath, VulExternalModule &out_module) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to load XML file '") + filepath + "': " + result.description());
    }

    pugi::xml_node root = doc.child("modulebase");
    if (!root) {
        return EStr(EItemXMLRootMissing, string("Missing root element 'modulebase' in XML file '") + filepath + "'");
    }
    return _parseModuleBase(root, out_module, filepath);
}

ErrorMsg _writeReqSrvNode(pugi::xml_node &node, const VulReqServ &rs) {
    pugi::xml_node rs_name = node.append_child("name");
    rs_name.append_child(pugi::node_pcdata).set_value(rs.name.c_str());

    if (!rs.comment.empty()) {
        pugi::xml_node rs_comment = node.append_child("comment");
        rs_comment.append_child(pugi::node_pcdata).set_value(rs.comment.c_str());
    }

    if (rs.has_handshake) {
        node.append_child("handshake");
    }

    for (const auto &arg : rs.args) {
        pugi::xml_node arg_node = node.append_child("arg");

        pugi::xml_node aname = arg_node.append_child("name");
        aname.append_child(pugi::node_pcdata).set_value(arg.name.c_str());

        pugi::xml_node atype = arg_node.append_child("type");
        atype.append_child(pugi::node_pcdata).set_value(arg.type.c_str());

        if (!arg.comment.empty()) {
            pugi::xml_node acomment = arg_node.append_child("comment");
            acomment.append_child(pugi::node_pcdata).set_value(arg.comment.c_str());
        }
    }

    for (const auto &ret : rs.rets) {
        pugi::xml_node ret_node = node.append_child("ret");

        pugi::xml_node rname = ret_node.append_child("name");
        rname.append_child(pugi::node_pcdata).set_value(ret.name.c_str());

        pugi::xml_node rtype = ret_node.append_child("type");
        rtype.append_child(pugi::node_pcdata).set_value(ret.type.c_str());

        if (!ret.comment.empty()) {
            pugi::xml_node rcomment = ret_node.append_child("comment");
            rcomment.append_child(pugi::node_pcdata).set_value(ret.comment.c_str());
        }
    }
    return "";
}

ErrorMsg _writeModuleBase(pugi::xml_node &root, const VulModuleBase &module) {
    // version
    pugi::xml_node version_node = root.append_child("version");
    char version_buf[64];
    snprintf(version_buf, sizeof(version_buf), "%u.%u.%u",
             std::get<0>(MemberVersion),
             std::get<1>(MemberVersion),
             std::get<2>(MemberVersion));
    version_node.append_child(pugi::node_pcdata).set_value(version_buf);

    // name
    pugi::xml_node name_node = root.append_child("name");
    name_node.append_child(pugi::node_pcdata).set_value(module.name.c_str());

    // comment (optional)
    if (!module.comment.empty()) {
        pugi::xml_node comment_node = root.append_child("comment");
        comment_node.append_child(pugi::node_pcdata).set_value(module.comment.c_str());
    }

    // localconf entries
    for (const auto &lce : module.local_configs) {
        const auto &lc = lce.second;
        pugi::xml_node lc_node = root.append_child("localconf");

        pugi::xml_node lc_name = lc_node.append_child("name");
        lc_name.append_child(pugi::node_pcdata).set_value(lc.name.c_str());

        pugi::xml_node lc_value = lc_node.append_child("value");
        lc_value.append_child(pugi::node_pcdata).set_value(lc.value.c_str());

        if (!lc.comment.empty()) {
            pugi::xml_node lc_comment = lc_node.append_child("comment");
            lc_comment.append_child(pugi::node_pcdata).set_value(lc.comment.c_str());
        }
    }

    // localbundle entries
    for (const auto &bundlee : module.local_bundles) {
        const auto &bundle = bundlee.second;
        pugi::xml_node bundle_node = root.append_child("localbundle");
        ErrorMsg err = _writeBundleNode(bundle_node, bundle);
        if (!err.empty()) {
            return err;
        }
    }

    // request entries
    for (const auto &reqe : module.requests) {
        pugi::xml_node req_node = root.append_child("request");
        const auto &req = reqe.second;
        ErrorMsg err = _writeReqSrvNode(req_node, req);
        if (!err.empty()) {
            return err;
        }
    }

    // service entries
    for (const auto &serve : module.services) {
        pugi::xml_node serv_node = root.append_child("service");
        const auto &serv = serve.second;
        ErrorMsg err = _writeReqSrvNode(serv_node, serv);
        if (!err.empty()) {
            return err;
        }
    }

    // pipein entries
    for (const auto &pine : module.pipe_inputs) {
        const auto &pin = pine.second;
        pugi::xml_node pin_node = root.append_child("pipein");

        pugi::xml_node pname = pin_node.append_child("name");
        pname.append_child(pugi::node_pcdata).set_value(pin.name.c_str());

        if (!pin.comment.empty()) {
            pugi::xml_node pcomment = pin_node.append_child("comment");
            pcomment.append_child(pugi::node_pcdata).set_value(pin.comment.c_str());
        }

        pugi::xml_node ptype = pin_node.append_child("type");
        ptype.append_child(pugi::node_pcdata).set_value(pin.type.c_str());
    }

    // pipeout entries
    for (const auto &poute : module.pipe_outputs) {
        const auto &pout = poute.second;
        pugi::xml_node pout_node = root.append_child("pipeout");

        pugi::xml_node pname = pout_node.append_child("name");
        pname.append_child(pugi::node_pcdata).set_value(pout.name.c_str());

        if (!pout.comment.empty()) {
            pugi::xml_node pcomment = pout_node.append_child("comment");
            pcomment.append_child(pugi::node_pcdata).set_value(pout.comment.c_str());
        }

        pugi::xml_node ptype = pout_node.append_child("type");
        ptype.append_child(pugi::node_pcdata).set_value(pout.type.c_str());
    }

    return "";
}

/**
 * @brief Write a base module to an XML file.
 * @param filepath The path to the XML file.
 * @param module The VulExternalModule to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeModuleBaseToXMLFile(const string &filepath, const VulExternalModule &module) {
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("modulebase");
    ErrorMsg err = _writeModuleBase(root, module);
    if (!err.empty()) {
        return err;
    }
    bool saveSucceeded = doc.save_file(filepath.c_str(), PUGIXML_TEXT("\t"), pugi::format_default | pugi::format_no_declaration);
    if (!saveSucceeded) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to save XML file '") + filepath + "'");
    }
    return "";
}

/*
ModuleVersion: 1.0

```
module
|-- version : 版本号
|-- name : 模块名称
|-- (comment) : 模块描述（可选）
|-- [localconf]
    |-- name : 本地配置项名称
    |-- value : 本地配置项值（或表达式）
    |-- (comment) : 本地配置项注释（可选）
|-- [request/service]
    |-- name : 请求名称
    |-- (comment) : 请求描述（可选）
    |-- (array_size) : 请求数组大小（可选，非数组请求时为空）
    |-- (handshake) : 是否包含握手（可选，默认为 false）
    |-- [arg]
        |-- name : 参数名称
        |-- type : 参数类型
        |-- (comment) : 参数描述（可选）
    |-- [ret]
        |-- name : 返回值名称
        |-- type : 返回值类型
        |-- (comment) : 返回值描述（可选）
|-- [pipein/pipeout]
    |-- name : 端口名称
    |-- (comment) : 端口描述（可选）
    |-- type : 管道端口数据类型
|-- [instance]
    |-- name : 模块实例名称
    |-- (comment) : 模块实例描述（可选）
    |-- type : 模块实例类型
    |-- [config]
        |-- name : 配置项名称
        |-- value : 配置项值（或表达式）
|-- [pipe]
    |-- name : 管道名称
    |-- (comment) : 管道描述（可选）
    |-- type : 管道数据类型
    |-- (inputsize) : 管道输入缓冲区大小（可选，默认1）
    |-- (outputsize) : 管道输出缓冲区大小（可选，默认1）
    |-- (buffersize) : 管道总缓冲区大小（可选，默认0）
    |-- (latency) : 管道延迟（可选，默认1）
    |-- (handshake) : 是否启用握手（可选，默认false）
    |-- (valid) : 是否启用数据有效标志（可选，默认false）
|-- [storage/storagenext/storagetmp]
    |-- name : 成员名称
    |-- (type) : 成员类型（可选）
    |-- (uintlen) : 成员类型位宽（可选，针对整数类型）
    |-- (value) : 成员值（可选，枚举类型时为必须）
    |-- (comment) : 成员描述（可选）
    |-- [dims] : 成员数组维度（可选，重复时代表多维）
|-- [reqconn/pipeconn]
    |-- frominstance : 源实例名称
    |-- fromport : 源端口名称
    |-- toinstance : 目标实例名称
    |-- toport : 目标端口名称
|-- [stallconn/seqconn]
    |-- frominstance : 源实例名称
    |-- toinstance : 目标实例名称
|-- (userheadercode)： 用户自定义头文件代码行Base64（可选）
|-- [servcode]
    |-- name : 服务名称
    |-- code : 代码行Base64
|-- [childreqcode]
    |-- instname : 子模块实例名称
    |-- reqname : 请求名称
    |-- code : 代码行Base64
|-- [tickcode]
    |-- name : 名称
    |-- (comment) : 注释
    |-- code : 代码行Base64
```
*/

ErrorMsg _parseStorageNode(pugi::xml_node &node, VulStorage &storage, const string &filepath) {
    pugi::xml_node s_name = node.child("name");
    if (!s_name) {
        return EStr(EItemXMLRequestMissing, string("Missing 'name' element in a storage in XML file '") + filepath + "'");
    }
    storage.name = s_name.text().as_string();

    if (pugi::xml_node s_type = node.child("type")) storage.type = s_type.text().as_string(); else storage.type = "";
    if (pugi::xml_node s_uintlen = node.child("uintlen")) storage.uint_length = s_uintlen.text().as_string(); else storage.uint_length = "";
    if (pugi::xml_node s_value = node.child("value")) storage.value = s_value.text().as_string(); else storage.value = "";
    if (pugi::xml_node s_comment = node.child("comment")) storage.comment = s_comment.text().as_string(); else storage.comment = "";

    for (pugi::xml_node dim_node : node.children("dim")) {
        string dim_str = dim_node.text().as_string();
        storage.dims.push_back(dim_str);
    }

    return "";
}

/**
 * @brief Parse a full module from an XML file.
 * @param filepath The path to the XML file.
 * @param out_module Output parameter to hold the parsed VulModule.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseModuleFromXMLFile(const string &filepath, VulModule &out_module) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to load XML file '") + filepath + "': " + result.description());
    }

    pugi::xml_node root = doc.child("module");
    if (!root) {
        return EStr(EItemXMLRootMissing, string("Missing root element 'module' in XML file '") + filepath + "'");
    }

    ErrorMsg err = _parseModuleBase(root, out_module, filepath);
    if (!err.empty()) {
        return err;
    }

    // instance entries
    for (pugi::xml_node inst_node : root.children("instance")) {
        VulInstance inst;
        pugi::xml_node iname = inst_node.child("name");
        if (!iname) return EStr(EItemXMLRequestMissing, string("Missing 'name' in instance in XML file '") + filepath + "'");
        inst.name = iname.text().as_string();
        if (pugi::xml_node icomment = inst_node.child("comment")) inst.comment = icomment.text().as_string(); else inst.comment = "";
        pugi::xml_node itype = inst_node.child("type");
        if (!itype) return EStr(EItemXMLRequestMissing, string("Missing 'type' in instance '") + inst.name + "' in XML file '" + filepath + "'");
        inst.module_name = itype.text().as_string();
        for (pugi::xml_node cfg_node : inst_node.children("config")) {
            pugi::xml_node cfname = cfg_node.child("name");
            if (!cfname) return EStr(EItemXMLRequestMissing, string("Missing 'name' in config override of instance '") + inst.name + "' in XML file '" + filepath + "'");
            string lcname = cfname.text().as_string();
            pugi::xml_node cfval = cfg_node.child("value");
            if (!cfval) return EStr(EItemXMLRequestMissing, string("Missing 'value' in config '") + lcname + "' in XML file '" + filepath + "'");
            string lcvalue = cfval.text().as_string();
            inst.local_config_overrides[lcname] = lcvalue;
        }
        out_module.instances[inst.name] = std::move(inst);
    }

    // pipe entries
    for (pugi::xml_node pipe_node : root.children("pipe")) {
        VulPipe pipe;
        pugi::xml_node pname = pipe_node.child("name");
        if (!pname) return EStr(EItemXMLRequestMissing, string("Missing 'name' in pipe in XML file '") + filepath + "'");
        pipe.name = pname.text().as_string();
        if (pugi::xml_node pcomment = pipe_node.child("comment")) pipe.comment = pcomment.text().as_string(); else pipe.comment = "";
        pugi::xml_node ptype = pipe_node.child("type");
        if (!ptype) return EStr(EItemXMLRequestMissing, string("Missing 'type' in pipe '") + pipe.name + "' in XML file '" + filepath + "'");
        pipe.type = ptype.text().as_string();
        if (pugi::xml_node pinsize = pipe_node.child("inputsize")) pipe.input_size = pinsize.text().as_string(); else pipe.input_size = "1";
        if (pugi::xml_node poutsize = pipe_node.child("outputsize")) pipe.output_size = poutsize.text().as_string(); else pipe.output_size = "1";
        if (pugi::xml_node pbufsize = pipe_node.child("buffersize")) pipe.buffer_size = pbufsize.text().as_string(); else pipe.buffer_size = "0";
        if (pugi::xml_node platency = pipe_node.child("latency")) pipe.latency = platency.text().as_string(); else pipe.latency = "1";
        pipe.has_handshake = pipe_node.child("handshake");
        pipe.has_valid = pipe_node.child("valid");
        out_module.pipe_instances[pipe.name] = std::move(pipe);
    }

    // storage entries
    for (pugi::xml_node stor_node : root.children("storage")) {
        VulStorage storage;
        ErrorMsg e = _parseStorageNode(stor_node, storage, filepath);
        if (!e.empty()) return e;
        out_module.storages[storage.name] = std::move(storage);
    }
    for (pugi::xml_node stor_node : root.children("storagenext")) {
        VulStorage storage;
        ErrorMsg e = _parseStorageNode(stor_node, storage, filepath);
        if (!e.empty()) return e;
        out_module.storagenexts[storage.name] = std::move(storage);
    }
    for (pugi::xml_node stor_node : root.children("storagetmp")) {
        VulStorage storage;
        ErrorMsg e = _parseStorageNode(stor_node, storage, filepath);
        if (!e.empty()) return e;
        out_module.storagetmp[storage.name] = std::move(storage);
    }

    // reqconn/pipeconn (connection entries)
    for (pugi::xml_node conn_node : root.children("reqconn")) {
        VulReqServConnection conn;
        pugi::xml_node cfrom = conn_node.child("frominstance");
        if (!cfrom) return EStr(EItemXMLRequestMissing, string("Missing 'frominstance' in reqconn in XML file '") + filepath + "'");
        conn.req_instance = cfrom.text().as_string();
        pugi::xml_node cfromp = conn_node.child("fromport");
        if (!cfromp) return EStr(EItemXMLRequestMissing, string("Missing 'fromport' in reqconn in XML file '") + filepath + "'");
        conn.req_name = cfromp.text().as_string();
        pugi::xml_node cto = conn_node.child("toinstance");
        if (!cto) return EStr(EItemXMLRequestMissing, string("Missing 'toinstance' in reqconn in XML file '") + filepath + "'");
        conn.serv_instance = cto.text().as_string();
        pugi::xml_node ctop = conn_node.child("toport");
        if (!ctop) return EStr(EItemXMLRequestMissing, string("Missing 'toport' in reqconn in XML file '") + filepath + "'");
        conn.serv_name = ctop.text().as_string();
        out_module.req_connections[conn.req_instance].insert(std::move(conn));
    }
    for (pugi::xml_node conn_node : root.children("pipeconn")) {
        VulModulePipeConnection conn;
        pugi::xml_node cfrom = conn_node.child("frominstance");
        if (!cfrom) return EStr(EItemXMLRequestMissing, string("Missing 'frominstance' in pipeconn in XML file '") + filepath + "'");
        conn.instance = cfrom.text().as_string();
        pugi::xml_node cfromp = conn_node.child("fromport");
        if (!cfromp) return EStr(EItemXMLRequestMissing, string("Missing 'fromport' in pipeconn in XML file '") + filepath + "'");
        conn.instance_pipe_port = cfromp.text().as_string();
        pugi::xml_node cto = conn_node.child("toinstance");
        if (!cto) return EStr(EItemXMLRequestMissing, string("Missing 'toinstance' in pipeconn in XML file '") + filepath + "'");
        conn.pipe_instance = cto.text().as_string();
        pugi::xml_node ctop = conn_node.child("toport");
        if (!ctop) return EStr(EItemXMLRequestMissing, string("Missing 'toport' in pipeconn in XML file '") + filepath + "'");
        conn.top_pipe_port = ctop.text().as_string();
        out_module.mod_pipe_connections[conn.instance].insert(std::move(conn));
    }

    // stallconn/seqconn (sequence connection entries)
    for (pugi::xml_node sconn_node : root.children("stallconn")) {
        VulSequenceConnection sconn;
        pugi::xml_node sfrom = sconn_node.child("frominstance");
        if (!sfrom) return EStr(EItemXMLRequestMissing, string("Missing 'frominstance' in stallconn in XML file '") + filepath + "'");
        sconn.former_instance = sfrom.text().as_string();
        pugi::xml_node sto = sconn_node.child("toinstance");
        if (!sto) return EStr(EItemXMLRequestMissing, string("Missing 'toinstance' in stallconn in XML file '") + filepath + "'");
        sconn.latter_instance = sto.text().as_string();
        out_module.stalled_connections.insert(std::move(sconn));
    }
    for (pugi::xml_node sconn_node : root.children("seqconn")) {
        VulSequenceConnection sconn;
        pugi::xml_node sfrom = sconn_node.child("frominstance");
        if (!sfrom) return EStr(EItemXMLRequestMissing, string("Missing 'frominstance' in seqconn in XML file '") + filepath + "'");
        sconn.former_instance = sfrom.text().as_string();
        pugi::xml_node sto = sconn_node.child("toinstance");
        if (!sto) return EStr(EItemXMLRequestMissing, string("Missing 'toinstance' in seqconn in XML file '") + filepath + "'");
        sconn.latter_instance = sto.text().as_string();
        out_module.update_constraints.insert(std::move(sconn));
    }

    // userheadercode (optional, Base64 encoded)
    if (pugi::xml_node uhc_node = root.child("userheadercode")) {
        string b64_uhc = uhc_node.text().as_string();
        auto decoded_lines = base64DecodeLines(b64_uhc);
        if (decoded_lines) {
            out_module.user_header_field_codelines.swap(*decoded_lines);
        } else {
            out_module.user_header_field_codelines.clear();
        }
    }

    // codeblock entries
    for (pugi::xml_node cb_node : root.children("servcode")) {
        pugi::xml_node cb_name = cb_node.child("name");
        if (!cb_name) return EStr(EItemXMLRequestMissing, string("Missing 'name' in servcode in XML file '") + filepath + "'");
        ReqServName servname = cb_name.text().as_string();
        pugi::xml_node cb_code = cb_node.child("code");
        if (!cb_code) return EStr(EItemXMLRequestMissing, string("Missing 'code' in servcode '") + servname + "' in XML file '" + filepath + "'");
        string b64_code = cb_code.text().as_string();
        auto decoded_lines = base64DecodeLines(b64_code);
        if (decoded_lines) {
            out_module.serv_codelines[servname] = std::move(*decoded_lines);
        } else {
            out_module.serv_codelines[servname].clear();
        }
    }
    for (pugi::xml_node cb_node : root.children("childreqcode")) {
        pugi::xml_node cb_iname = cb_node.child("instname");
        if (!cb_iname) return EStr(EItemXMLRequestMissing, string("Missing 'instname' in childreqcode in XML file '") + filepath + "'");
        InstanceName instname = cb_iname.text().as_string();
        pugi::xml_node cb_rname = cb_node.child("reqname");
        if (!cb_rname) return EStr(EItemXMLRequestMissing, string("Missing 'reqname' in childreqcode in XML file '") + filepath + "'");
        ReqServName reqname = cb_rname.text().as_string();
        pugi::xml_node cb_code = cb_node.child("code");
        if (!cb_code) return EStr(EItemXMLRequestMissing, string("Missing 'code' in childreqcode for instance '") + instname + "' and request '" + reqname + "' in XML file '" + filepath + "'");
        string b64_code = cb_code.text().as_string();
        auto decoded_lines = base64DecodeLines(b64_code);
        if (decoded_lines) {
            out_module.req_codelines[instname][reqname] = std::move(*decoded_lines);
        } else {
            out_module.req_codelines[instname][reqname].clear();
        }
    }
    for (pugi::xml_node cb_node : root.children("tickcode")) {
        VulTickCodeBlock tcb;
        pugi::xml_node cb_name = cb_node.child("name");
        if (!cb_name) return EStr(EItemXMLRequestMissing, string("Missing 'name' in tickcode in XML file '") + filepath + "'");
        tcb.name = cb_name.text().as_string();
        if (pugi::xml_node cb_comment = cb_node.child("comment")) tcb.comment = cb_comment.text().as_string(); else tcb.comment = "";
        pugi::xml_node cb_code = cb_node.child("code");
        if (!cb_code) return EStr(EItemXMLRequestMissing, string("Missing 'code' in tickcode '") + tcb.name + "' in XML file '" + filepath + "'");
        string b64_code = cb_code.text().as_string();
        auto decoded_lines = base64DecodeLines(b64_code);
        if (decoded_lines) {
            tcb.codelines = std::move(*decoded_lines);
        } else {
            tcb.codelines.clear();
        }
        out_module.user_tick_codeblocks[tcb.name] = std::move(tcb);
    }

    return "";
}

ErrorMsg _writeStorageNode(pugi::xml_node &node, const VulStorage &storage) {
    pugi::xml_node s_name = node.append_child("name");
    s_name.append_child(pugi::node_pcdata).set_value(storage.name.c_str());

    if (!storage.type.empty()) {
        pugi::xml_node s_type = node.append_child("type");
        s_type.append_child(pugi::node_pcdata).set_value(storage.type.c_str());
    }
    if (!storage.uint_length.empty()) {
        pugi::xml_node s_uintlen = node.append_child("uintlen");
        s_uintlen.append_child(pugi::node_pcdata).set_value(storage.uint_length.c_str());
    }
    if (!storage.value.empty()) {
        pugi::xml_node s_value = node.append_child("value");
        s_value.append_child(pugi::node_pcdata).set_value(storage.value.c_str());
    }
    if (!storage.comment.empty()) {
        pugi::xml_node s_comment = node.append_child("comment");
        s_comment.append_child(pugi::node_pcdata).set_value(storage.comment.c_str());
    }

    for (const auto &dim : storage.dims) {
        pugi::xml_node dim_node = node.append_child("dim");
        dim_node.append_child(pugi::node_pcdata).set_value(dim.c_str());
    }

    return "";
}

/**
 * @brief Write a full module to an XML file.
 * @param filepath The path to the XML file.
 * @param module The VulModule to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeModuleToXMLFile(const string &filepath, const VulModule &module) {
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("module");
    ErrorMsg err = _writeModuleBase(root, module);
    if (!err.empty()) {
        return err;
    }

    // instance entries
    for (const auto &inste : module.instances) {
        const auto &inst = inste.second;
        pugi::xml_node inst_node = root.append_child("instance");
        pugi::xml_node iname = inst_node.append_child("name");
        iname.append_child(pugi::node_pcdata).set_value(inst.name.c_str());
        if (!inst.comment.empty()) {
            pugi::xml_node icomment = inst_node.append_child("comment");
            icomment.append_child(pugi::node_pcdata).set_value(inst.comment.c_str());
        }
        pugi::xml_node itype = inst_node.append_child("type");
        itype.append_child(pugi::node_pcdata).set_value(inst.module_name.c_str());
        for (const auto &cfg : inst.local_config_overrides) {
            pugi::xml_node cfg_node = inst_node.append_child("config");
            pugi::xml_node cfname = cfg_node.append_child("name");
            cfname.append_child(pugi::node_pcdata).set_value(cfg.first.c_str());
            pugi::xml_node cfval = cfg_node.append_child("value");
            cfval.append_child(pugi::node_pcdata).set_value(cfg.second.c_str());
        }
    }

    // pipe entries
    for (const auto &pipee : module.pipe_instances) {
        const auto &pipe = pipee.second;
        pugi::xml_node pipe_node = root.append_child("pipe");
        pugi::xml_node pname = pipe_node.append_child("name");
        pname.append_child(pugi::node_pcdata).set_value(pipe.name.c_str());
        if (!pipe.comment.empty()) {
            pugi::xml_node pcomment = pipe_node.append_child("comment");
            pcomment.append_child(pugi::node_pcdata).set_value(pipe.comment.c_str());
        }
        pugi::xml_node ptype = pipe_node.append_child("type");
        ptype.append_child(pugi::node_pcdata).set_value(pipe.type.c_str());
        if (!pipe.input_size.empty()) {
            pugi::xml_node pinsize = pipe_node.append_child("inputsize");
            pinsize.append_child(pugi::node_pcdata).set_value(pipe.input_size.c_str());
        }
        if (!pipe.output_size.empty()) {
            pugi::xml_node poutsize = pipe_node.append_child("outputsize");
            poutsize.append_child(pugi::node_pcdata).set_value(pipe.output_size.c_str());
        }
        if (!pipe.buffer_size.empty()) {
            pugi::xml_node pbufsize = pipe_node.append_child("buffersize");
            pbufsize.append_child(pugi::node_pcdata).set_value(pipe.buffer_size.c_str());
        }
        if (!pipe.latency.empty()) {
            pugi::xml_node platency = pipe_node.append_child("latency");
            platency.append_child(pugi::node_pcdata).set_value(pipe.latency.c_str());
        }
        if (pipe.has_handshake) pipe_node.append_child("handshake");
        if (pipe.has_valid) pipe_node.append_child("valid");
    }

    // storage entries
    for (const auto &store : module.storages) {
        pugi::xml_node stor_node = root.append_child("storage");
        ErrorMsg e = _writeStorageNode(stor_node, store.second);
        if (!e.empty()) {
            return e;
        }
    }
    for (const auto &store : module.storagenexts) {
        pugi::xml_node stor_node = root.append_child("storagenext");
        ErrorMsg e = _writeStorageNode(stor_node, store.second);
        if (!e.empty()) {
            return e;
        }
    }
    for (const auto &store : module.storagetmp) {
        pugi::xml_node stor_node = root.append_child("storagetmp");
        ErrorMsg e = _writeStorageNode(stor_node, store.second);
        if (!e.empty()) {
            return e;
        }
    }

    // reqconn entries
    for (const auto &conne : module.req_connections) {
        for (const auto &conn : conne.second) {
            pugi::xml_node conn_node = root.append_child("reqconn");
            pugi::xml_node cfrom = conn_node.append_child("frominstance");
            cfrom.append_child(pugi::node_pcdata).set_value(conn.req_instance.c_str());
            pugi::xml_node cfromp = conn_node.append_child("fromport");
            cfromp.append_child(pugi::node_pcdata).set_value(conn.req_name.c_str());
            pugi::xml_node cto = conn_node.append_child("toinstance");
            cto.append_child(pugi::node_pcdata).set_value(conn.serv_instance.c_str());
            pugi::xml_node ctop = conn_node.append_child("toport");
            ctop.append_child(pugi::node_pcdata).set_value(conn.serv_name.c_str());
        }
    }

    // pipeconn entries
    for (const auto &conne : module.mod_pipe_connections) {
        for (const auto &conn : conne.second) {
            pugi::xml_node conn_node = root.append_child("pipeconn");
            pugi::xml_node cfrom = conn_node.append_child("frominstance");
            cfrom.append_child(pugi::node_pcdata).set_value(conn.instance.c_str());
            pugi::xml_node cfromp = conn_node.append_child("fromport");
            cfromp.append_child(pugi::node_pcdata).set_value(conn.instance_pipe_port.c_str());
            pugi::xml_node cto = conn_node.append_child("toinstance");
            cto.append_child(pugi::node_pcdata).set_value(conn.pipe_instance.c_str());
            pugi::xml_node ctop = conn_node.append_child("toport");
            ctop.append_child(pugi::node_pcdata).set_value(conn.top_pipe_port.c_str());
        }
    }

    // stallconn entries
    for (const auto &sconn : module.stalled_connections) {
        pugi::xml_node sconn_node = root.append_child("stallconn");
        pugi::xml_node sfrom = sconn_node.append_child("frominstance");
        sfrom.append_child(pugi::node_pcdata).set_value(sconn.former_instance.c_str());
        pugi::xml_node sto = sconn_node.append_child("toinstance");
        sto.append_child(pugi::node_pcdata).set_value(sconn.latter_instance.c_str());
    }

    // seqconn entries
    for (const auto &sconn : module.update_constraints) {
        pugi::xml_node sconn_node = root.append_child("seqconn");
        pugi::xml_node sfrom = sconn_node.append_child("frominstance");
        sfrom.append_child(pugi::node_pcdata).set_value(sconn.former_instance.c_str());
        pugi::xml_node sto = sconn_node.append_child("toinstance");
        sto.append_child(pugi::node_pcdata).set_value(sconn.latter_instance.c_str());
    }

    // userheadercode (optional, Base64 encoded)
    if (!module.user_header_field_codelines.empty()) {
        string b64_uhc = base64Encode(module.user_header_field_codelines);
        pugi::xml_node uhc_node = root.append_child("userheadercode");
        uhc_node.append_child(pugi::node_pcdata).set_value(b64_uhc.c_str());
    }

    // codeblock entries
    for (const auto &cbe : module.user_tick_codeblocks) {
        const auto &cb = cbe.second;
        pugi::xml_node cb_node = root.append_child("tickcode");
        pugi::xml_node cb_name = cb_node.append_child("name");
        cb_name.append_child(pugi::node_pcdata).set_value(cb.name.c_str());
        if (!cb.comment.empty()) {
            pugi::xml_node cb_comment = cb_node.append_child("comment");
            cb_comment.append_child(pugi::node_pcdata).set_value(cb.comment.c_str());
        }
        string b64_code = base64Encode(cb.codelines);
        pugi::xml_node cb_code = cb_node.append_child("code");
        cb_code.append_child(pugi::node_pcdata).set_value(b64_code.c_str());
    }
    for (const auto &cbe : module.serv_codelines) {
        const auto &servname = cbe.first;
        const auto &codelines = cbe.second;
        pugi::xml_node cb_node = root.append_child("servcode");
        pugi::xml_node cb_name = cb_node.append_child("name");
        cb_name.append_child(pugi::node_pcdata).set_value(servname.c_str());
        string b64_code = base64Encode(codelines);
        pugi::xml_node cb_code = cb_node.append_child("code");
        cb_code.append_child(pugi::node_pcdata).set_value(b64_code.c_str());
    }
    for (const auto &cbe : module.req_codelines) {
        const auto &instname = cbe.first;
        for (const auto &reqe : cbe.second) {
            const auto &reqname = reqe.first;
            const auto &codelines = reqe.second;
            pugi::xml_node cb_node = root.append_child("childreqcode");
            pugi::xml_node cb_iname = cb_node.append_child("instname");
            cb_iname.append_child(pugi::node_pcdata).set_value(instname.c_str());
            pugi::xml_node cb_rname = cb_node.append_child("reqname");
            cb_rname.append_child(pugi::node_pcdata).set_value(reqname.c_str());
            string b64_code = base64Encode(codelines);
            pugi::xml_node cb_code = cb_node.append_child("code");
            cb_code.append_child(pugi::node_pcdata).set_value(b64_code.c_str());
        }
    }

    // Save to file
    bool saveSucceeded = doc.save_file(filepath.c_str(), PUGIXML_TEXT("\t"), pugi::format_default | pugi::format_no_declaration);
    if (!saveSucceeded) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to save XML file '") + filepath + "'");
    }

    return "";
}

/*

ProjectVersion: 1.0

```
project
|-- version : 版本号
|-- [import]
    |-- abspath : 导入项目绝对路径，为空则使用VULLIB环境变量
    |-- name : 导入模块名称
    |-- [configoverride]
        |-- name : 配置项名称
        |-- value : 配置项值（或表达式）
|-- topmodule : 顶层模块名称
|-- [module]
```
*/

static const tuple<uint32_t, uint32_t, uint32_t> ProjectVersion = {1, 0, 0};

/**
 * @brief Parse a project from an XML file.
 * @param filepath The path to the XML file.
 * @param out_project Output parameter to hold the parsed VulProjectRaw.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseProjectFromXMLFile(const string &filepath, VulProjectRaw &out_project) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to load XML file '") + filepath + "': " + result.description());
    }

    pugi::xml_node root = doc.child("project");
    if (!root) {
        return EStr(EItemXMLRootMissing, string("Missing root element 'project' in XML file '") + filepath + "'");
    }

    // version
    pugi::xml_node version_node = root.child("version");
    if (!version_node) {
        return EStr(EItemXMLRequestMissing, string("Missing 'version' element in XML file '") + filepath + "'");
    }
    {
        string version_str = version_node.text().as_string();
        uint32_t major = 0, minor = 0, patch = 0;
        if (sscanf(version_str.c_str(), "%u.%u.%u", &major, &minor, &patch) < 2) {
            return EStr(EItemXMLRequestMissing, string("Invalid 'version' format in XML file '") + filepath + "': " + version_str);
        }
        if (make_tuple(major, minor, patch) != ProjectVersion) {
            return EStr(EItemXMLVersionMismatch, string("Version mismatch in XML file '") + filepath + "': expected 1.0.0, got " + version_str);
        }
    }

    // import entries (optional)
    for (pugi::xml_node import_node : root.children("import")) {
        VulImportRaw imp;
        pugi::xml_node abspath_node = import_node.child("abspath");
        if (!abspath_node) {
            return EStr(EItemXMLRequestMissing, string("Missing 'abspath' in import in XML file '") + filepath + "'");
        }
        imp.abspath = abspath_node.text().as_string();

        pugi::xml_node name_node = import_node.child("name");
        if (!name_node) {
            return EStr(EItemXMLRequestMissing, string("Missing 'name' in import in XML file '") + filepath + "'");
        }
        imp.name = name_node.text().as_string();

        // configoverride entries (optional)
        for (pugi::xml_node cfg_node : import_node.children("configoverride")) {
            pugi::xml_node cfg_name = cfg_node.child("name");
            if (!cfg_name) {
                return EStr(EItemXMLRequestMissing, string("Missing 'name' in configoverride in import '") + imp.name + "' in XML file '" + filepath + "'");
            }
            ConfigName name = cfg_name.text().as_string();

            pugi::xml_node cfg_value = cfg_node.child("value");
            if (!cfg_value) {
                return EStr(EItemXMLRequestMissing, string("Missing 'value' in configoverride '") + name + "' in import '" + imp.name + "' in XML file '" + filepath + "'");
            }
            ConfigValue value = cfg_value.text().as_string();

            imp.config_overrides[name] = value;
        }

        out_project.imports.push_back(std::move(imp));
    }

    // topmodule (required)
    pugi::xml_node topmodule_node = root.child("topmodule");
    if (!topmodule_node) {
        return EStr(EItemXMLRequestMissing, string("Missing 'topmodule' element in XML file '") + filepath + "'");
    }
    out_project.top_module = topmodule_node.text().as_string();

    for (pugi::xml_node module_node : root.children("module")) {
        out_project.modules.push_back(module_node.text().as_string());
    }

    return "";
}

/**
 * @brief Write a project to an XML file.
 * @param filepath The path to the XML file.
 * @param project The VulProjectRaw to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeProjectToXMLFile(const string &filepath, const VulProjectRaw &project) {
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("project");

    // version
    pugi::xml_node version_node = root.append_child("version");
    char version_buf[64];
    snprintf(version_buf, sizeof(version_buf), "%u.%u.%u",
             std::get<0>(ProjectVersion),
             std::get<1>(ProjectVersion),
             std::get<2>(ProjectVersion));
    version_node.append_child(pugi::node_pcdata).set_value(version_buf);

    // import entries (optional)
    for (const auto &imp : project.imports) {
        pugi::xml_node import_node = root.append_child("import");
        pugi::xml_node abspath_node = import_node.append_child("abspath");
        abspath_node.append_child(pugi::node_pcdata).set_value(imp.abspath.c_str());
        pugi::xml_node name_node = import_node.append_child("name");
        name_node.append_child(pugi::node_pcdata).set_value(imp.name.c_str());
        for (const auto &cfg : imp.config_overrides) {
            pugi::xml_node cfg_node = import_node.append_child("configoverride");
            pugi::xml_node cfg_name = cfg_node.append_child("name");
            cfg_name.append_child(pugi::node_pcdata).set_value(cfg.first.c_str());
            pugi::xml_node cfg_value = cfg_node.append_child("value");
            cfg_value.append_child(pugi::node_pcdata).set_value(cfg.second.c_str());
        }
    }

    // topmodule (required)
    pugi::xml_node topmodule_node = root.append_child("topmodule");
    topmodule_node.append_child(pugi::node_pcdata).set_value(project.top_module.c_str());

    for (const auto &module_name : project.modules) {
        pugi::xml_node module_node = root.append_child("module");
        module_node.append_child(pugi::node_pcdata).set_value(module_name.c_str());
    }

    // Save to file
    bool saveSucceeded = doc.save_file(filepath.c_str(), PUGIXML_TEXT("\t"), pugi::format_default | pugi::format_no_declaration);
    if (!saveSucceeded) {
        return EStr(EItemXMLFileOpenFailed, string("Failed to save XML file '") + filepath + "'");
    }

    return "";
}

} // namespace serialize
