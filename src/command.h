/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "design.h"

// Command functions to modify a VulDesign object.
// Any modification to the design SHOULD use these functions to ensure consistency.
// All functions return an empty string on success, or an error message on failure.

// For config items

/**
 * @brief Add or update a config item in the design's config library.
 * If the config item already exists, its value and comment will be updated.
 * @param design The VulDesign object to modify.
 * @param name The name of the config item.
 * @param value The value of the config item.
 * @param comment An optional comment for the config item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateConfigItem(VulDesign &design, const string &name, const string &value, const string &comment);

/**
 * @brief Rename a config item in the design's config library.
 * All combines using this config item will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the config item.
 * @param newname The new name of the config item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameConfigItem(VulDesign &design, const string &oldname, const string &newname);

// For bundles

/**
 * @brief Add or update a bundle in the design.
 * If the bundle already exists, its comment will be updated.
 * @param design The VulDesign object to modify.
 * @param name The name of the bundle.
 * @param comment An optional comment for the bundle.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateBundle(VulDesign &design, const string &name, const string &comment);

/**
 * @brief Rename a bundle in the design.
 * All combines and pipes using this bundle will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the bundle.
 * @param newname The new name of the bundle.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameBundle(VulDesign &design, const string &oldname, const string &newname);

/**
 * @brief Add, update, or remove a member in a bundle.
 * If the member already exists, its type and comment will be updated.
 * If membertype is empty, the member will be removed.
 * @param design The VulDesign object to modify.
 * @param bundlename The name of the bundle to modify.
 * @param membername The name of the member to add, update, or remove.
 * @param membertype The type of the member. If empty, the member will be removed.
 * @param comment An optional comment for the member.
 * @param value An optional default value for the member (not used currently).
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupBundleMember(VulDesign &design, const string &bundlename, const string &membername, const string &membertype, const string &comment, const string &value);

// For combines

/**
 * @brief Add or update a combine in the design.
 * If the combine already exists, its comment and stallable flag will be updated.
 * @param design The VulDesign object to modify.
 * @param name The name of the combine.
 * @param comment An optional comment for the combine.
 * @param stallable Whether the combine is stallable.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateCombine(VulDesign &design, const string &name, const string &comment, bool stallable);

/**
 * @brief Rename a combine in the design.
 * All instances using this combine will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the combine.
 * @param newname The new name of the combine.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombine(VulDesign &design, const string &oldname, const string &newname);

/**
 * @brief Duplicate a combine in the design.
 * All instances using the old combine will not be affected.
 * @param design The VulDesign object to modify.
 * @param oldname The name of the existing combine to duplicate.
 * @param newname The name of the new combine to create.
 * @return An empty string on success, or an error message on failure.
 */
string cmdDuplicateCombine(VulDesign &design, const string &oldname, const string &newname);

/**
 * @brief Add, update, or remove a config item in a combine.
 * If the config item already exists, its ref and comment will be updated.
 * If configref is empty, the config item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param configname The name of the config item to add, update, or remove.
 * @param configvalue The value string for the config item. It can reference config items by name.
 *                  If empty, the config item will be removed.
 * @param comment An optional comment for the config item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineConfig(VulDesign &design, const string &combinename, const string &configname, const string &configvalue, const string &comment);

/**
 * @brief Add, update, or remove a storage item in a combine.
 * If the storage item already exists, its type, value, and comment will be updated.
 * If storagetype is empty, the storage item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param storagename The name of the storage item to add, update, or remove.
 * @param storagetype The type of the storage item. If empty, the storage item will be removed.
 * @param value An optional initial value for the storage item.
 * @param comment An optional comment for the storage item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineStorage(VulDesign &design, const string &combinename, const string &storagename, const string &storagetype, const string &value, const string &comment);

/**
 * @brief Add, update, or remove a storage-next item in a combine.
 * If the storage-next item already exists, its type, value, and comment will be updated.
 * If storagetype is empty, the storage-next item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param storagename The name of the storage-next item to add, update, or remove.
 * @param storagetype The type of the storage-next item. If empty, the storage-next item will be removed.
 * @param value An optional initial value for the storage-next item.
 * @param comment An optional comment for the storage-next item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineStorageNext(VulDesign &design, const string &combinename, const string &storagename, const string &storagetype, const string &value, const string &comment);

/**
 * @brief Add, update, or remove a storage-tick item in a combine.
 * If the storage-tick item already exists, its type, value, and comment will be updated.
 * If storagetype is empty, the storage-tick item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param storagename The name of the storage-tick item to add, update, or remove.
 * @param storagetype The type of the storage-tick item. If empty, the storage-tick item will be removed.
 * @param value An optional initial value for the storage-tick item.
 * @param comment An optional comment for the storage-tick item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineStorageTick(VulDesign &design, const string &combinename, const string &storagename, const string &storagetype, const string &value, const string &comment);

/**
 * @brief Add, update, or remove a storage-next item in a combine.
 * If the storage-next item already exists, its type, value, and comment will be updated.
 * If storagetype is empty, the storage-next item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param storagename The name of the storage-next item to add, update, or remove.
 * @param storagetype The type of the storage-next item. If empty, the storage-next item will be removed.
 * @param storagesize The size of the storage-next item. Cannot be empty if storagetype is not empty.
 * @param value An optional initial value for the storage-next item.
 * @param comment An optional comment for the storage-next item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineStorageNextArray(VulDesign &design, const string &combinename, const string &storagename, const string &storagetype, const string &storagesize, const string &value, const string &comment);

/**
 * @brief Set up the tick function for a combine.
 * If all parameters are empty, the tick function will be disabled.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param code The raw C++ code for the tick function.
 * @param file The file name containing the tick function.
 * @param funcname The name of the tick function.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineTickFunction(VulDesign &design, const string &combinename, const string &code, const string &file, const string &funcname);

/**
 * @brief Set up the applytick function for a combine.
 * If all parameters are empty, the applytick function will be disabled.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param code The raw C++ code for the applytick function.
 * @param file The file name containing the applytick function.
 * @param funcname The name of the applytick function.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineApplyTickFunction(VulDesign &design, const string &combinename, const string &code, const string &file, const string &funcname);

/**
 * @brief Set up the init function for a combine.
 * If all parameters are empty, the init function will be disabled.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param code The raw C++ code for the init function.
 * @param file The file name containing the init function.
 * @param funcname The name of the init function.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineInitFunction(VulDesign &design, const string &combinename, const string &code, const string &file, const string &funcname);

/**
 * @brief Add, update, or remove a pipe input port in a combine.
 * If the pipe input port already exists, its type and comment will be updated.
 * If pipetype is empty, the pipe input port will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param pipename The name of the pipe input port to add, update, or remove.
 * @param pipetype The type of the pipe input port. If empty, the pipe input port will be removed.
 * @param comment An optional comment for the pipe input port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombinePipeIn(VulDesign &design, const string &combinename, const string &pipename, const string &pipetype, const string &comment);

/**
 * @brief Rename a pipe input port in a combine.
 * All connections using this pipe input port will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param oldpipename The old name of the pipe input port.
 * @param newpipename The new name of the pipe input port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombinePipeIn(VulDesign &design, const string &combinename, const string &oldpipename, const string &newpipename);

/**
 * @brief Add, update, or remove a pipe output port in a combine.
 * If the pipe output port already exists, its type and comment will be updated.
 * If pipetype is empty, the pipe output port will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param pipename The name of the pipe output port to add, update, or remove.
 * @param pipetype The type of the pipe output port. If empty, the pipe output port will be removed.
 * @param comment An optional comment for the pipe output port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombinePipeOut(VulDesign &design, const string &combinename, const string &pipename, const string &pipetype, const string &comment);

/**
 * @brief Rename a pipe output port in a combine.
 * All connections using this pipe output port will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param oldpipename The old name of the pipe output port.
 * @param newpipename The new name of the pipe output port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombinePipeOut(VulDesign &design, const string &combinename, const string &oldpipename, const string &newpipename);

/**
 * @brief Add or update a request port in a combine.
 * If the request port already exists, its arguments, return values, and comment will be updated.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param reqname The name of the request port to add, update, or remove.
 * @param args The arguments for the request port.
 * @param rets The return values for the request port.
 * @param comment An optional comment for the request port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateCombineRequest(VulDesign &design, const string &combinename, const string &reqname, const vector<VulArgument> &args, const vector<VulArgument> &rets, const string &comment);

/**
 * @brief Rename a request port in a combine.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param oldreqname The old name of the request port.
 * @param newreqname The new name of the request port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombineRequest(VulDesign &design, const string &combinename, const string &oldreqname, const string &newreqname);

/**
 * @brief Add or update a service port in a combine.
 * If the service port already exists, its arguments, return values, and comment will be updated.
 * For a new service port, its associated cpp file and function name will be created automatically.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param servname The name of the service port to add, update, or remove.
 * @param args The arguments for the service port.
 * @param rets The return values for the service port.
 * @param comment An optional comment for the service port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateCombineService(VulDesign &design, const string &combinename, const string &servname, const vector<VulArgument> &args, const vector<VulArgument> &rets, const string &comment);

/**
 * @brief Rename a service port in a combine.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param oldservname The old name of the service port.
 * @param newservname The new name of the service port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombineService(VulDesign &design, const string &combinename, const string &oldservname, const string &newservname);

// For instances

/**
 * @brief Add or update an instance in the design.
 * If the instance already exists, its combine and comment will be updated.
 * @param design The VulDesign object to modify.
 * @param name The name of the instance.
 * @param combine The name of the combine this instance refers to.
 * @param comment An optional comment for the instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateInstance(VulDesign &design, const string &name, const string &combine, const string &comment);

/**
 * @brief Rename an instance in the design.
 * All connections involving this instance will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the instance.
 * @param newname The new name of the instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameInstance(VulDesign &design, const string &oldname, const string &newname);

/**
 * @brief Set up a config override for an instance.
 * If the config override already exists, its value will be updated.
 * If configvalue is empty, the config override will be removed.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance to modify.
 * @param configname The name of the config item to add, update, or remove.
 * @param configvalue The override value for the config item. If empty, the config override will be removed.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupInstanceConfigOverride(VulDesign &design, const string &instancename, const string &configname, const string &configvalue);

// For pipes

/**
 * @brief Add, update or remove a pipe in the design.
 * If the pipe already exists, its type, comment, input size, output size, and buffer size will be updated.
 * If pipetype is empty, the pipe will be removed.
 * @param design The VulDesign object to modify.
 * @param name The name of the pipe.
 * @param type The data type of the pipe.
 * @param comment An optional comment for the pipe.
 * @param inputsize The input size of the pipe (0 for unlimited).
 * @param outputsize The output size of the pipe (0 for unlimited).
 * @param buffersize The buffer size of the pipe (0 for default).
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdatePipe(VulDesign &design, const string &name, const string &type, const string &comment, unsigned int inputsize, unsigned int outputsize, unsigned int buffersize);

/**
 * @brief Rename a pipe in the design.
 * All connections using this pipe will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the pipe.
 * @param newname The new name of the pipe.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenamePipe(VulDesign &design, const string &oldname, const string &newname);

// For connections

/**
 * @brief Connect a module's output port to a pipe's input port.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance (module) to connect from.
 * @param pipeoutport The name of the module's output port to connect from.
 * @param pipename The name of the pipe to connect to.
 * @param portindex The index of the pipe's input port to connect to (0-based).
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectModuleToPipe(VulDesign &design, const string &instancename, const string &pipeoutport, const string &pipename, unsigned int portindex);

/**
 * @brief Disconnect a module's output port from a pipe's input port.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance (module) to disconnect from.
 * @param pipeoutport The name of the module's output port to disconnect from.
 * @param pipename The name of the pipe to disconnect from.
 * @param portindex The index of the pipe's input port to disconnect from (0-based).
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectModuleFromPipe(VulDesign &design, const string &instancename, const string &pipeoutport, const string &pipename, unsigned int portindex);

/**
 * @brief Connect a pipe's output port to a module's input port.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance (module) to connect to.
 * @param pipeinport The name of the module's input port to connect to.
 * @param pipename The name of the pipe to connect from.
 * @param portindex The index of the pipe's output port to connect from (0-based).
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectPipeToModule(VulDesign &design, const string &instancename, const string &pipeinport, const string &pipename, unsigned int portindex);

/**
 * @brief Disconnect a pipe's output port from a module's input port.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance (module) to disconnect from.
 * @param pipeinport The name of the module's input port to disconnect from.
 * @param pipename The name of the pipe to disconnect from.
 * @param portindex The index of the pipe's output port to disconnect from (0-based).
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectPipeFromModule(VulDesign &design, const string &instancename, const string &pipeinport, const string &pipename, unsigned int portindex);

/**
 * @brief Connect a request port of one module to a service port of another module.
 * @param design The VulDesign object to modify.
 * @param reqinstname The name of the instance (module) with the request port.
 * @param reqportname The name of the request port to connect from.
 * @param servinstname The name of the instance (module) with the service port.
 * @param servportname The name of the service port to connect to.
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectReqToServ(VulDesign &design, const string &reqinstname, const string &reqportname, const string &servinstname, const string &servportname);

/**
 * @brief Disconnect a request port of one module from a service port of another module.
 * @param design The VulDesign object to modify.
 * @param reqinstname The name of the instance (module) with the request port.
 * @param reqportname The name of the request port to disconnect from.
 * @param servinstname The name of the instance (module) with the service port.
 * @param servportname The name of the service port to disconnect from.
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectReqFromServ(VulDesign &design, const string &reqinstname, const string &reqportname, const string &servinstname, const string &servportname);

/**
 * @brief Connect two instances in a stalled connection.
 * Both instances must be from stallable combines.
 * @param design The VulDesign object to modify.
 * @param srcinstname The name of the source instance.
 * @param destinstname The name of the destination instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectStalled(VulDesign &design, const string &srcinstname, const string &destinstname);

/**
 * @brief Disconnect two instances in a stalled connection.
 * @param design The VulDesign object to modify.
 * @param srcinstname The name of the source instance.
 * @param destinstname The name of the destination instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectStalled(VulDesign &design, const string &srcinstname, const string &destinstname);

/**
 * @brief Connect two instances in a sequence connection.
 * @param design The VulDesign object to modify.
 * @param formerinstname The name of the former instance.
 * @param latterinstname The name of the latter instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectUpdateSequence(VulDesign &design, const string &formerinstname, const string &latterinstname);

/**
 * @brief Disconnect two instances in a sequence connection.
 * @param design The VulDesign object to modify.
 * @param formerinstname The name of the former instance.
 * @param latterinstname The name of the latter instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectUpdateSequence(VulDesign &design, const string &formerinstname, const string &latterinstname);


/**
 * @brief Update all C++ file helpers for init/tick/applytick/service ports in the design.
 * This function will create or update C++ files and function names for all service ports
 * that do not have them set up yet. It will also check for any broken files and report them.
 * @param design The VulDesign object to modify.
 * @param outputbrokenfiles A vector to store the names of any broken files found during the update.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateAllCppFileHelpers(VulDesign &design, vector<string> &outputbrokenfiles);

/**
 * @brief Update C++ file helpers for init/tick/applytick/service ports in a specific combine.
 * This function will create or update C++ files and function names for all service ports
 * in the specified combine that do not have them set up yet. It will also check for any broken files and report them.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to update.
 * @param outputbrokenfiles A vector to store the names of any broken files found during the update.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateCombineCppFileHelpers(VulDesign &design, const string &combinename, vector<string> &outputbrokenfiles);




