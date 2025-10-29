
#include "SimpleMemoryBlock64.h"

#include <fstream>
#include <sstream>
#include <cstring>

SimpleMemoryBlock64::SimpleMemoryBlock64(ConstructorArguments & arg) {
    this->BaseAddress = arg.BaseAddress;
    this->SizeMegaBytes = arg.SizeMegaBytes;
    this->__instance_name = arg.__instance_name;

    assertf(this->BaseAddress >= 0, "SimpleMemoryBlock64 %s: BaseAddress must be greater than or equal to 0", this->__instance_name.c_str());
    assertf(this->SizeMegaBytes > 0, "SimpleMemoryBlock64 %s: SizeMegaBytes must be greater than 0", this->__instance_name.c_str());
    __block_data.resize((SizeMegaBytes * 1024UL * 1024UL) / sizeof(uint64), 0);

    initialize_from_json();
}

void SimpleMemoryBlock64::initialize_from_json() {
    // init memory from instance_name.json if exists
    std::string json_path = this->__instance_name + ".json";
    if (!std::filesystem::exists(json_path)) return;

    std::unordered_map<uint64, std::string> addr_file_map;
    {
        // extract <baseaddress, filepath> key-value from json
        std::ifstream ifs(json_path, std::ios::in | std::ios::binary);
        if (!ifs) {
            // unable to open json even though it exists -> treat as empty
            printf("Warning: unable to open %s. Skipping memory initialization.\n", json_path.c_str());
            return;
        }
        std::ostringstream ss;
        ss << ifs.rdbuf();
        std::string s = ss.str();

        size_t pos = 0;
        auto skip_ws = [&](void){
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) ++pos;
        };

        while (true) {
            skip_ws();
            if (pos >= s.size()) break;

            // find key
            std::string key_str;
            if (s[pos] == '"') {
                size_t k2 = s.find('"', pos + 1);
                if (k2 == std::string::npos) break;
                key_str = s.substr(pos + 1, k2 - pos - 1);
                pos = k2 + 1;
            } else {
                // unquoted key: read until ':' or whitespace or comma or '}'
                size_t k2 = pos;
                while (k2 < s.size() && s[k2] != ':' && s[k2] != ',' && s[k2] != ' ' && s[k2] != '\t' && s[k2] != '\r' && s[k2] != '\n' && s[k2] != '}') ++k2;
                if (k2 == pos) break;
                key_str = s.substr(pos, k2 - pos);
                pos = k2;
            }

            // find ':'
            skip_ws();
            if (pos >= s.size()) break;
            if (s[pos] == ':') {
                ++pos;
            } else {
                size_t colon = s.find(':', pos);
                if (colon == std::string::npos) break;
                pos = colon + 1;
            }

            // find value (expect string)
            skip_ws();
            if (pos >= s.size()) break;
            std::string value_str;
            if (s[pos] == '"') {
                size_t v2 = s.find('"', pos + 1);
                if (v2 == std::string::npos) break;
                value_str = s.substr(pos + 1, v2 - pos - 1);
                pos = v2 + 1;
            } else {
                // allow unquoted value until comma or closing brace
                size_t v2 = pos;
                while (v2 < s.size() && s[v2] != ',' && s[v2] != '}' && s[v2] != '\n' && s[v2] != '\r') ++v2;
                value_str = s.substr(pos, v2 - pos);
                // trim trailing whitespace
                size_t a = 0; while (a < value_str.size() && (value_str[a] == ' ' || value_str[a] == '\t')) ++a;
                size_t b = value_str.size(); while (b > a && (value_str[b-1] == ' ' || value_str[b-1] == '\t')) --b;
                value_str = value_str.substr(a, b - a);
                pos = v2;
            }

            // validate key is a number (decimal or 0x hex)
            try {
                size_t idx = 0;
                uint64 addr = std::stoull(key_str, &idx, 0);
                // ensure entire key consumed
                assertf(idx == key_str.size(), "invalid numeric key '%s' in %s", key_str.c_str(), json_path.c_str());
                addr_file_map[addr] = value_str;
            } catch (...) {
                assertf(false, "invalid numeric key '%s' in %s", key_str.c_str(), json_path.c_str());
            }

            // advance to next possible pair (skip until comma or continue)
            skip_ws();
            if (pos < s.size() && s[pos] == ',') ++pos;
        }
    }

    uint8 *pdata8 = (uint8 *)(this->__block_data.data());
    uint64 baseaddr = BaseAddress;
    uint64 blocksize = (SizeMegaBytes * 1024UL * 1024UL);
    // load each file into memory at specified base address
    for (const auto & [load_addr, filepath] : addr_file_map) {
        // check file exists
        assertf(std::filesystem::exists(filepath), "%s: referenced file '%s' not found", this->__instance_name.c_str(), filepath.c_str());

        std::ifstream fin(filepath, std::ios::in | std::ios::binary);
        if (!fin) {
            assertf(false, "%s: failed to open '%s'", this->__instance_name.c_str(), filepath.c_str());
        }

        fin.seekg(0, std::ios::end);
        std::streamoff sfsize = fin.tellg();
        if (sfsize <= 0) {
            // empty file, nothing to load
            continue;
        }
        uint64 fsize = static_cast<uint64>(sfsize);

        // compute overlap between file mapping [load_addr, load_addr+fsize) and block [baseaddr, baseaddr+blocksize)
        uint64 file_start = load_addr;
        uint64 file_end = load_addr + fsize; // exclusive
        uint64 block_start = baseaddr;
        uint64 block_end = baseaddr + blocksize; // exclusive

        // no overlap
        if (file_end <= block_start || file_start >= block_end) continue;

        uint64 copy_start = (file_start < block_start) ? block_start : file_start;
        uint64 copy_end = (file_end > block_end) ? block_end : file_end;
        uint64 bytes_to_copy = copy_end - copy_start;

        // offsets
        uint64 file_offset = copy_start - file_start; // within file
        uint64 dest_offset = copy_start - block_start; // within block pdata8

        // sanity
        assertf((dest_offset + bytes_to_copy) <= blocksize, "copy would overflow block (dest_offset=%llu bytes=%llu blocksize=%llu)", (unsigned long long)dest_offset, (unsigned long long)bytes_to_copy, (unsigned long long)blocksize);

        // read from file
        fin.seekg(static_cast<std::streamoff>(file_offset), std::ios::beg);
        std::vector<uint8> buf(static_cast<size_t>(bytes_to_copy));
        fin.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(bytes_to_copy));
        std::streamsize actually_read = fin.gcount();
        if (actually_read <= 0) continue;

        // copy into block
        std::memcpy(pdata8 + dest_offset, buf.data(), static_cast<size_t>(actually_read));
    }
}


SimpleMemoryBlock64::~SimpleMemoryBlock64() {
}

void SimpleMemoryBlock64::all_current_tick() {
}

void SimpleMemoryBlock64::all_current_applytick() {
}

/*
* memory_load
* @arg <address> address
* @ret <data> data
*/
void SimpleMemoryBlock64::memory_load(uint64 address, uint64 * data) {
    if (address < BaseAddress || address >= (BaseAddress + (SizeMegaBytes * 1024UL * 1024UL))) {
        // out of range
        *data = 0;
        return;
    }

    uint64 offset = (address - BaseAddress) / 8; // uint64 index
    if (offset < (SizeMegaBytes * 1024UL * 1024UL)) {
        *data = __block_data[offset];
    } else {
        *data = 0;
    }
}

/*
* memory_store
* @arg <address> address
* @arg <data> data
*/
void SimpleMemoryBlock64::memory_store(uint64 address, uint64 data) {
    if (address < BaseAddress || address >= (BaseAddress + (SizeMegaBytes * 1024UL * 1024UL))) {
        // out of range
        return;
    }

    uint64 offset = (address - BaseAddress) / 8; // uint64 index
    if (offset < (SizeMegaBytes * 1024UL * 1024UL)) {
        __block_data[offset] = data;
    }
}


