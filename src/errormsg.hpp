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

constexpr uint32_t EOPCreate = 30070;
constexpr uint32_t EOPCreateAlreadyOpened = 30071;
constexpr uint32_t EOPCreateMissArg = 30072;
constexpr uint32_t EOPCreateNameExists = 30073;
constexpr uint32_t EOPCreateFileFailed = 30074;

constexpr uint32_t EOPInfo= 30080;
constexpr uint32_t EOPInfoNotOpened = 30081;

constexpr uint32_t EOPUndo = 30090;
constexpr uint32_t EOPUndoFailed = 30091;
constexpr uint32_t EOPRedoFailed = 30092;

constexpr uint32_t EOPConfAdd = 30200;
constexpr uint32_t EOPConfAddMissArg = 30201;
constexpr uint32_t EOPConfAddNameInvalid = 30202;
constexpr uint32_t EOPConfAddNameConflict = 30203;
constexpr uint32_t EOPConfAddValueInvalid = 30204;
constexpr uint32_t EOPConfAddRefNotFound = 30205;

constexpr uint32_t EOPConfRename = 30210;
constexpr uint32_t EOPConfRenameMissArg = 30211;
constexpr uint32_t EOPConfRenameNameNotFound = 30212;
constexpr uint32_t EOPConfRenameNameInvalid = 30213;
constexpr uint32_t EOPConfRenameNameConflict = 30214;
constexpr uint32_t EOPConfRenameReferenced = 30215;
constexpr uint32_t EOPConfRenameImport = 30216;

constexpr uint32_t EOPConfComment = 30220;
constexpr uint32_t EOPConfCommentMissArg = 30221;
constexpr uint32_t EOPConfCommentNameNotFound = 30222;
constexpr uint32_t EOPConfCommentImport = 30223;

constexpr uint32_t EOPConfUpdate = 30230;
constexpr uint32_t EOPConfUpdateMissArg = 30231;
constexpr uint32_t EOPConfUpdateNameNotFound = 30232;
constexpr uint32_t EOPConfUpdateValueInvalid = 30233;
constexpr uint32_t EOPConfUpdateRefLoop = 30234;

constexpr uint32_t EOPConfRemove = 30240;
constexpr uint32_t EOPConfRemoveMissArg = 30241;
constexpr uint32_t EOPConfRemoveNameNotFound = 30242;
constexpr uint32_t EOPConfRemoveReferenced = 30243;
constexpr uint32_t EOPConfRemoveImport = 30244;

constexpr uint32_t EOPConfListRef = 30250;
constexpr uint32_t EOPConfListRefMissArg = 30251;
constexpr uint32_t EOPConfListRefNameNotFound = 30252;

constexpr uint32_t EOPBundAdd = 30400;
constexpr uint32_t EOPBundAddMissArg = 30401;
constexpr uint32_t EOPBundAddNameInvalid = 30402;
constexpr uint32_t EOPBundAddNameConflict = 30403;
constexpr uint32_t EOPBundAddDefinitionInvalid = 30404;

constexpr uint32_t EOPBundListRef = 30410;
constexpr uint32_t EOPBundListRefMissArg = 30411;
constexpr uint32_t EOPBundListRefNameNotFound = 30412;

constexpr uint32_t EOPBundRename = 30420;
constexpr uint32_t EOPBundRenameMissArg = 30421;
constexpr uint32_t EOPBundRenameNameNotFound = 30422;
constexpr uint32_t EOPBundRenameNameConflict = 30423;
constexpr uint32_t EOPBundRenameReferenced = 30424;
constexpr uint32_t EOPBundRenameImport = 30425;

constexpr uint32_t EOPBundComment = 30430;
constexpr uint32_t EOPBundCommentMissArg = 30431;
constexpr uint32_t EOPBundCommentNameNotFound = 30432;

constexpr uint32_t EOPBundUpdate = 30440;
constexpr uint32_t EOPBundUpdateMissArg = 30441;
constexpr uint32_t EOPBundUpdateNameNotFound = 30442;
constexpr uint32_t EOPBundUpdateDefinitionInvalid = 30443;
constexpr uint32_t EOPBundUpdateImport = 30444;
constexpr uint32_t EOPBundUpdateRefLoop = 30445;

constexpr uint32_t EOPBundRemove = 30450;
constexpr uint32_t EOPBundRemoveMissArg = 30451;
constexpr uint32_t EOPBundRemoveNameNotFound = 30452;
constexpr uint32_t EOPBundRemoveReferenced = 30453;
constexpr uint32_t EOPBundRemoveImport = 30454;

constexpr uint32_t EOPModAdd = 31000;
constexpr uint32_t EOPModAddMissArg = 31001;
constexpr uint32_t EOPModAddNameInvalid = 31002;
constexpr uint32_t EOPModAddNameConflict = 31003;

constexpr uint32_t EOPModRemove = 31010;
constexpr uint32_t EOPModRemoveMissArg = 31011;
constexpr uint32_t EOPModRemoveNameNotFound = 31012;
constexpr uint32_t EOPModRemoveImport = 31013;
constexpr uint32_t EOPModRemoveNotEmpty = 31014;

constexpr uint32_t EOPModRename = 31020;
constexpr uint32_t EOPModRenameMissArg = 31021;
constexpr uint32_t EOPModRenameNameNotFound = 31022;
constexpr uint32_t EOPModRenameNameInvalid = 31023;
constexpr uint32_t EOPModRenameNameConflict = 31024;
constexpr uint32_t EOPModRenameImport = 31025;
constexpr uint32_t EOPModRenameReferenced = 31026;

constexpr uint32_t EOPModCommonMissArg = 31030;
constexpr uint32_t EOPModCommonNotFound = 31031;
constexpr uint32_t EOPModCommonImport = 31032;

constexpr uint32_t EOPModConn = 31100;
constexpr uint32_t EOPModConnMissArg = 31101;
constexpr uint32_t EOPModConnSrcInstNotFound = 31102;
constexpr uint32_t EOPModConnSrcPortNotFound = 31103;
constexpr uint32_t EOPModConnDstInstNotFound = 31104;
constexpr uint32_t EOPModConnDstPortNotFound = 31105;
constexpr uint32_t EOPModConnMismatch = 31106;
constexpr uint32_t EOPModConnMultiConn = 31107;
constexpr uint32_t EOPModConnAlreadyExists = 31108;

constexpr uint32_t EOPModPConn = 31120;
constexpr uint32_t EOPModPConnMissArg = 31121;
constexpr uint32_t EOPModPConnSrcInstNotFound = 31122;
constexpr uint32_t EOPModPConnSrcPortNotFound = 31123;
constexpr uint32_t EOPModPConnDstPipeNotFound = 31124;
constexpr uint32_t EOPModPConnDstPortNotFound = 31125;
constexpr uint32_t EOPModPConnMismatch = 31126;
constexpr uint32_t EOPModPConnMultiConn = 31127;
constexpr uint32_t EOPModPConnAlreadyExists = 31128;

constexpr uint32_t EOPModSConn = 31140;
constexpr uint32_t EOPModSConnMissArg = 31141;
constexpr uint32_t EOPModSConnSrcInstNotFound = 31142;
constexpr uint32_t EOPModSConnDstInstNotFound = 31143;
constexpr uint32_t EOPModSConnSelfLoop = 31144;
constexpr uint32_t EOPModSConnLoop = 31145;

constexpr uint32_t EOPModUConn = 31160;
constexpr uint32_t EOPModUConnMissArg = 31161;
constexpr uint32_t EOPModUConnSrcInstNotFound = 31162;
constexpr uint32_t EOPModUConnDstInstNotFound = 31163;
constexpr uint32_t EOPModUConnSelfLoop = 31164;
constexpr uint32_t EOPModUConnLoop = 31165;

constexpr uint32_t EOPModReqAdd = 31200;
constexpr uint32_t EOPModReqAddReqNameDup = 31201;
constexpr uint32_t EOPModReqAddReqNameInvalid = 31202;

constexpr uint32_t EOPModReqRename = 31210;
constexpr uint32_t EOPModReqRenameReqNotFound = 31211;
constexpr uint32_t EOPModReqRenameReqNameDup = 31212;
constexpr uint32_t EOPModReqRenameReqNameInvalid = 31213;
constexpr uint32_t EOPModReqRenameConnected = 31214;

constexpr uint32_t EOPModReqGet = 31220;
constexpr uint32_t EOPModReqGetReqNotFound = 31221;
constexpr uint32_t EOPModReqGetServNotFound = 31222;

constexpr uint32_t EOPModReqUpdate = 31230;
constexpr uint32_t EOPModReqUpdateNotFound = 31231;
constexpr uint32_t EOPModReqUpdateConnected = 31232;

constexpr uint32_t EOPModReqRemove = 31240;
constexpr uint32_t EOPModReqRemoveNotFound = 31241;
constexpr uint32_t EOPModReqRemoveConnected = 31242;

constexpr uint32_t EOPModPipePortAdd = 31300;
constexpr uint32_t EOPModPipePortAddNameDup = 31301;
constexpr uint32_t EOPModPipePortAddNameInvalid = 31302;

constexpr uint32_t EOPModPipePortRename = 31310;
constexpr uint32_t EOPModPipePortRenameNotFound = 31311;
constexpr uint32_t EOPModPipePortRenameNameDup = 31312;
constexpr uint32_t EOPModPipePortRenameNameInvalid = 31313;
constexpr uint32_t EOPModPipePortRenameConnected = 31314;

constexpr uint32_t EOPModPipePortUpdate = 31320;
constexpr uint32_t EOPModPipePortUpdateNotFound = 31321;
constexpr uint32_t EOPModPipePortUpdateConnected = 31322;

constexpr uint32_t EOPModPipePortRemove = 31330;
constexpr uint32_t EOPModPipePortRemoveNotFound = 31331;
constexpr uint32_t EOPModPipePortRemoveConnected = 31332;

constexpr uint32_t EOPModConf = 31400;
constexpr uint32_t EOPModConfNotFound = 31401;
constexpr uint32_t EOPModConfNameInvalid = 31402;
constexpr uint32_t EOPModConfNameDup = 31403;
constexpr uint32_t EOPModConfValueInvalid = 31404;

constexpr uint32_t EOPModBund = 31500;
constexpr uint32_t EOPModBundNotFound = 31501;
constexpr uint32_t EOPModBundNameInvalid = 31502;
constexpr uint32_t EOPModBundNameDup = 31503;
constexpr uint32_t EOPModBundDefinitionInvalid = 31504;
constexpr uint32_t EOPModBundRefLoop = 31505;

constexpr uint32_t EOPModStorage = 31600;
constexpr uint32_t EOPModStorageNotFound = 31601;
constexpr uint32_t EOPModStorageNameInvalid = 31602;
constexpr uint32_t EOPModStorageNameDup = 31603;
constexpr uint32_t EOPModStorageDefinitionInvalid = 31604;

constexpr uint32_t EOPModInstance = 31700;
constexpr uint32_t EOPModInstanceNotFound = 31701;
constexpr uint32_t EOPModInstanceNameInvalid = 31702;
constexpr uint32_t EOPModInstanceNameDup = 31703;

constexpr uint32_t EOPModPipe = 31800;
constexpr uint32_t EOPModPipeNotFound = 31801;
constexpr uint32_t EOPModPipeNameInvalid = 31802;
constexpr uint32_t EOPModPipeNameDup = 31803;


constexpr uint32_t EGEN = 40000;
constexpr uint32_t EGENCancel = 40001;
constexpr uint32_t EGENTopNotFound = 40002;
constexpr uint32_t EGENModuleNotFound = 40003;
constexpr uint32_t EGENWriteFileFailed = 40004;
constexpr uint32_t EGENAlreadyRunning = 40005;
constexpr uint32_t EGENStepInvalid = 40006;
constexpr uint32_t EGENInvalidLibrary = 40007;
constexpr uint32_t EGENInvalidProject = 40008;
constexpr uint32_t EGENRunFormerStage = 40009;
constexpr uint32_t EGENNoTaskSpecified = 40010;
constexpr uint32_t EGENInvalidCXX = 40011;
constexpr uint32_t EGENCompilationFailed = 40012;
constexpr uint32_t EGENSimulationFailed = 40013;
