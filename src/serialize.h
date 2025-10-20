#pragma once

#include "type.h"

#include <unordered_map>

using std::unordered_map;

/**
 * @brief Parse project info from an XML file.
 * @param filename The name of the XML file to load.
 * @param vp The VulProject object to populate.
 * @return An error message string, empty on success.
 */
string serializeParseProjectInfoFromFile(const string &filename, VulProject &vp);

/**
 * @brief Save project info to an XML file.
 * @param filename The name of the XML file to save.
 * @param vp The VulProject object to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveProjectInfoToFile(const string &filename, const VulProject &vp);


/**
 * @brief Parse a VulBundle from an XML file.
 * @param filename The name of the XML file to load.
 * @param vb The VulBundle object to populate.
 * @return An error message string, empty on success.
 */
string serializeParseBundleFromFile(const string &filename, VulBundle &vb);

/**
 * @brief Save a VulBundle to an XML file.
 * @param filename The name of the XML file to save.
 * @param vb The VulBundle object to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveBundleToFile(const string &filename, const VulBundle &vb);


/**
 * @brief Parse VulConfigLib from an XML file.
 * @param filename The name of the XML file to load.
 * @param config_lib The unordered_map to populate with VulConfigItem objects.
 * @return An error message string, empty on success.
 */
string serializeParseConfigLibFromFile(const string &filename, unordered_map<string, VulConfigItem> &config_lib);

/**
 * @brief Save VulConfigLib to an XML file.
 * @param filename The name of the XML file to save.
 * @param config_lib The unordered_map to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveConfigLibToFile(const string &filename, const unordered_map<string, VulConfigItem> &config_lib);

/**
 * @brief Parse VulCombine from an XML file.
 * @param filename The name of the XML file to load.
 * @param vc The VulCombine object to populate.
 * @return An error message string, empty on success.
 */
string serializeParseCombineFromFile(const string &filename, VulCombine &vc);

/**
 * @brief Save VulCombine to an XML file.
 * @param filename The name of the XML file to save.
 * @param vc The VulCombine object to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveCombineToFile(const string &filename, const VulCombine &vc);

/**
 * @brief Parse VulPrefab from an XML file.
 * @param filename The name of the XML file to load.
 * @param prefab The VulPrefab object to populate.
 * @return An error message string, empty on success.
 */
string serializeParsePrefabFromFile(const string &filename, VulPrefab &prefab);

/**
 * @brief Save VulPrefab to an XML file.
 * @param filename The name of the XML file to save.
 * @param prefab The VulPrefab object to serialize.
 * @return An error message string, empty on success.
 */
string serializeSavePrefabToFile(const string &filename, const VulPrefab &prefab);

/**
 * @brief Parse VulDesign from an XML file.
 * @param filename The name of the XML file to load.
 * @param instances The unordered_map to populate with VulInstance objects.
 * @param pipes The unordered_map to populate with VulPipe objects.
 * @param req_connections The vector to populate with VulReqConnection objects.
 * @param mod_pipe_connections The vector to populate with VulModulePipeConnection objects.
 * @param pipe_mod_connections The vector to populate with VulPipeModuleConnection objects.
 * @param stalled_connections The vector to populate with VulStalledConnection objects.
 * @param update_constraints The vector to populate with VulUpdateSequence objects.
 * @param vis_blocks The vector to populate with VulVisBlock objects.
 * @param vis_texts The vector to populate with VulVisText objects.
 * @return An error message string, empty on success.
 */
string serializeParseDesignFromFile(const string &filename,
    unordered_map<string, VulInstance>  &instances,
    unordered_map<string, VulPipe>      &pipes,
    vector<VulReqConnection>            &req_connections,
    vector<VulModulePipeConnection>     &mod_pipe_connections,
    vector<VulPipeModuleConnection>     &pipe_mod_connections,
    vector<VulStalledConnection>        &stalled_connections,
    vector<VulUpdateSequence>           &update_constraints,
    vector<VulVisBlock>                 &vis_blocks,
    vector<VulVisText>                  &vis_texts
);

/**
 * @brief Save VulDesign to an XML file.
 * @param filename The name of the XML file to save.
 * @param instances The unordered_map of VulInstance objects to serialize.
 * @param pipes The unordered_map of VulPipe objects to serialize.
 * @param req_connections The vector of VulReqConnection objects to serialize.
 * @param mod_pipe_connections The vector of VulModulePipeConnection objects to serialize.
 * @param pipe_mod_connections The vector of VulPipeModuleConnection objects to serialize.
 * @param stalled_connections The vector of VulStalledConnection objects to serialize.
 * @param update_constraints The vector of VulUpdateSequence objects to serialize.
 * @param vis_blocks The vector of VulVisBlock objects to serialize.
 * @param vis_texts The vector of VulVisText objects to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveDesignToFile(const string &filename,
    const unordered_map<string, VulInstance>  &instances,
    const unordered_map<string, VulPipe>      &pipes,
    const vector<VulReqConnection>            &req_connections,
    const vector<VulModulePipeConnection>     &mod_pipe_connections,
    const vector<VulPipeModuleConnection>     &pipe_mod_connections,
    const vector<VulStalledConnection>        &stalled_connections,
    const vector<VulUpdateSequence>           &update_constraints,
    const vector<VulVisBlock>                 &vis_blocks,
    const vector<VulVisText>                  &vis_texts
);

