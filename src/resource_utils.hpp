#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include "project.h"

namespace vulresource {

// Resolve a project resource by filename. Resources are searched recursively;
// the match closest to the project root wins. Lexical ordering makes matches
// at the same depth deterministic across filesystems.
inline std::filesystem::path findShallowestProjectResource(
    const std::filesystem::path &project_dir,
    const std::filesystem::path &resource
) {
    const auto filename = resource.filename();
    if (filename.empty()) {
        throw VulException("Resource path does not name a file: " + resource.string());
    }

    std::optional<std::filesystem::path> best;
    std::size_t best_depth = 0;
    std::string best_relative;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(project_dir, ec), end;
    if (ec) {
        throw VulException("Failed to search project resources under " + project_dir.string() +
                           ": " + ec.message());
    }
    while (it != end) {
        if (!it->is_regular_file(ec)) {
            if (ec) {
                throw VulException("Failed while inspecting project resource " +
                                   it->path().string() + ": " + ec.message());
            }
        } else if (it->path().filename() == filename) {
            const auto relative = std::filesystem::relative(it->path(), project_dir, ec);
            if (ec) {
                throw VulException("Failed to resolve project resource path " + it->path().string() +
                                   ": " + ec.message());
            }
            std::size_t depth = 0;
            const auto parent = relative.parent_path();
            for (const auto &part : parent) {
                (void)part;
                ++depth;
            }
            const std::string relative_text = relative.generic_string();
            if (!best || depth < best_depth ||
                (depth == best_depth && relative_text < best_relative)) {
                best = it->path();
                best_depth = depth;
                best_relative = relative_text;
            }
        }
        it.increment(ec);
        if (ec) {
            throw VulException("Failed while searching project resources under " +
                               project_dir.string() + ": " + ec.message());
        }
    }

    if (!best) {
        throw VulException("Resource file '" + filename.string() +
                           "' was not found recursively under project directory: " +
                           project_dir.string());
    }
    return *best;
}

} // namespace vulresource
