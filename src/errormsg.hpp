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

#include <inttypes.h>

using std::string;

struct ErrorMsg {
    ErrorMsg() : msg(""), code(0) {}
    ErrorMsg(uint32_t c, const string &m) : msg(m), code(c) {}
    ErrorMsg(const string &m) : msg(m), code(0) {}
    ErrorMsg(uint32_t c, const char *m) : msg(m), code(c) {}
    ErrorMsg(const char *m) : msg(m), code(0) {}

    uint32_t code;
    string msg;

    bool empty() const {
        return msg.empty();
    }
    bool success() const {
        return code == 0;
    }
    bool error() const {
        return code != 0;
    }
    explicit operator bool() const {
        return code != 0;
    }
};

inline ErrorMsg EStr(uint32_t code, const string &msg) {
    return {code, msg};
}

inline ErrorMsg operator+(const ErrorMsg &a, const string &b) {
    return {a.code, a.msg + b};
}

inline ErrorMsg &operator+=(ErrorMsg &a, const string &b) {
    a.msg += b;
    return a;
}

constexpr uint32_t ErrLoadProject = 10000;
constexpr uint32_t ErrNameCheck = 10001;
constexpr uint32_t ErrTypeCheck = 10002;
constexpr uint32_t ErrReqConnCheck = 10003;
constexpr uint32_t ErrPipeConnCheck = 10004;
constexpr uint32_t ErrSequenceCheck = 10005;
constexpr uint32_t ErrSaveProject = 10006;
constexpr uint32_t ErrConfMod = 10010;
constexpr uint32_t ErrBundleMod = 10011;
constexpr uint32_t ErrCombineMod = 10012;
constexpr uint32_t ErrInstanceMod = 10013;
constexpr uint32_t ErrPipeMod = 10014;
constexpr uint32_t ErrConnMod = 10015;

constexpr uint32_t EItemOPInvalid = 20000;
constexpr uint32_t EItemOPLoadNotClosed = 20100;
constexpr uint32_t EItemOPLoadPathInvalid = 20101;
constexpr uint32_t EItemOPLoadCannotOpen = 20102;
constexpr uint32_t EItemOPLoadImportNotFound = 20103;
constexpr uint32_t EItemOPLoadCheckFailed = 20104;

constexpr uint32_t ErrConfLib = 21000;
constexpr uint32_t EItemConfNameDup = 21000;
constexpr uint32_t EItemConfNameInvalid = 21001;
constexpr uint32_t EItemConfNameNotFound = 21002;
constexpr uint32_t EItemConfGroupNameDup = 21003;
constexpr uint32_t EItemConfGroupNameInvalid = 21004;
constexpr uint32_t EItemConfGroupNameNotFound = 21005;
constexpr uint32_t EItemConfRefNotFound = 21006;
constexpr uint32_t EItemConfRefLooped = 21007;
constexpr uint32_t EItemConfRenameRef = 21008;
constexpr uint32_t EItemConfRemoveRef = 21009;
constexpr uint32_t EItemConfValueEmpty = 21010;
constexpr uint32_t EItemConfValueTokenInvalid = 21011;
constexpr uint32_t EItemConfValueGrammerInvalid = 21012;

constexpr uint32_t ErrBundLib = 22000;
constexpr uint32_t EItemBundNameDup = 22000;
constexpr uint32_t EItemBundNameInvalid = 22001;
constexpr uint32_t EItemBundNameNotFound = 22002;
constexpr uint32_t EItemBundTagDup = 22003;
constexpr uint32_t EItemBundTagInvalid = 22004;
constexpr uint32_t EItemBundTagNotFound = 22005;
constexpr uint32_t EItemBundRefNotFound = 22006;
constexpr uint32_t EItemBundRefLooped = 22007;
constexpr uint32_t EItemBundRenameRef = 22008;
constexpr uint32_t EItemBundRemoveRef = 22009;
constexpr uint32_t EItemBundTypeMixed = 22010;
constexpr uint32_t EItemBundEnumInvalid = 22011;
constexpr uint32_t EItemBundEnumValueDup = 22012;
constexpr uint32_t EItemBundEnumNameDup = 22013;
constexpr uint32_t EItemBundConstGrammarInvalid = 22014;
constexpr uint32_t EItemBundConfRefNotFound = 22015;
constexpr uint32_t EItemBundMemNameInvalid = 22016;
constexpr uint32_t EItemBundMemLengthInvalid = 22017;
constexpr uint32_t EItemBundRenameTagged = 22018;
constexpr uint32_t EItemBundUpdateTagged = 22019;
constexpr uint32_t EItemBundAliasInvalid = 22020;
constexpr uint32_t EItemBundInitWithValue = 22021;

constexpr uint32_t ErrModule = 23000;
constexpr uint32_t ErrModuleBrokenIndex = 23001;

constexpr uint32_t EItemModLocalNameDup = 23100;
constexpr uint32_t EItemModLocalNameInvalid = 23101;

constexpr uint32_t EItemModConfInvalidValue = 23200;
constexpr uint32_t EItemModConfRefNotFound = 23201;
constexpr uint32_t EItemModBundInvalidType = 23202;
constexpr uint32_t EItemModBundRefNotFound = 23203;
constexpr uint32_t EItemModBundRefLooped = 23204;
constexpr uint32_t EItemModBundInvalidEnum = 23205;
constexpr uint32_t EItemModBundMemNameInvalid = 23206;
constexpr uint32_t EItemModBundMemNameDup = 23207;
constexpr uint32_t EItemModBundMemRefNotFound = 23208;
constexpr uint32_t EItemModBundMemUnexpectedValue = 23209;

constexpr uint32_t EItemModInstRefNotFound = 23300;
constexpr uint32_t EItemModInstConfOverrideNotFound = 23301;

constexpr uint32_t EItemModRConnInvalidInst = 23400;
constexpr uint32_t EItemModRConnInvalidPort = 23401;
constexpr uint32_t EItemModRConnMismatch = 23402;
constexpr uint32_t EItemModRConnServMultiConn = 23403;
constexpr uint32_t EItemModRConnServNotConnected = 23404;
constexpr uint32_t EItemModRConnChildReqMultiConn = 23405;
constexpr uint32_t EItemModRConnChildReqNotConnected = 23406;

constexpr uint32_t EItemModPConnInvalidInst = 23500;
constexpr uint32_t EItemModPConnInvalidPort = 23501;
constexpr uint32_t EItemModPConnInvalidPipe = 23502;
constexpr uint32_t EItemModPConnMismatch = 23503;
constexpr uint32_t EItemModPConnAmbigPort = 23504;
constexpr uint32_t EItemModPConnChildPortNotConnected = 23505;
constexpr uint32_t EItemModPConnChildPortMultiConn = 23506;

constexpr uint32_t EItemModSConnInvalidInst = 23600;
constexpr uint32_t EItemModSConnInvalidTop = 23601;
constexpr uint32_t EItemModSConnSelfLoop = 23602;
constexpr uint32_t EItemModSConnLoop = 23603;
constexpr uint32_t EItemModUConnInvalidInst = 23604;
constexpr uint32_t EItemModUConnInvalidTop = 23605;
constexpr uint32_t EItemModUConnSelfLoop = 23606;
constexpr uint32_t EItemModUConnLoop = 23607;
constexpr uint32_t ErrXML = 24000;
constexpr uint32_t EItemXMLFileOpenFailed = 24000;
constexpr uint32_t EItemXMLVersionMismatch = 24001;
constexpr uint32_t EItemXMLRootMissing = 24002;
constexpr uint32_t EItemXMLRequestMissing = 24003;

constexpr uint32_t EOPLoad = 30000;
constexpr uint32_t EOPLoadMissArg = 30001;
constexpr uint32_t EOPLoadNotClosed = 30002;
constexpr uint32_t EOPLoadInvalidPath = 30003;
constexpr uint32_t EOPLoadSerializeFailed = 30004;
constexpr uint32_t EOPLoadImportNotFound = 30005;
constexpr uint32_t EOPLoadImportInvalidPath = 30006;
constexpr uint32_t EOPLoadBundleConflict = 30007;
constexpr uint32_t EOPLoadModuleConflict = 30008;
constexpr uint32_t EOPLoadNoTopModule = 30009;
constexpr uint32_t EOPLoadConfigInvalidValue = 30010;
constexpr uint32_t EOPLoadConfigLoopRef = 30011;
constexpr uint32_t EOPLoadBundleInvalidValue = 30012;
constexpr uint32_t EOPLoadBundleMissingConf = 30013;
constexpr uint32_t EOPLoadBundleLoopRef = 30014;
constexpr uint32_t EOPLoadModuleMissingInst = 30015;
constexpr uint32_t EOPLoadModuleLoopRef = 30016;

constexpr uint32_t EOPCancel = 30050;
constexpr uint32_t EOPCancelNotOpened = 30051;

constexpr uint32_t EOPSave = 30060;
constexpr uint32_t EOPSaveNotOpened = 30061;
constexpr uint32_t EOPSaveFileFailed = 30062;

