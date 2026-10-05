#pragma once

#include <cstdint>
#include <vector>

namespace nengine::render {

const std::vector<std::uint32_t>&
diagnostic_vertex_spirv();

const std::vector<std::uint32_t>&
diagnostic_fragment_spirv();

} // namespace nengine::render
