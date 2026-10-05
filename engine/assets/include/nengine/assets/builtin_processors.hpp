#pragma once

#include "nengine/assets/import_pipeline.hpp"

namespace nengine::assets {

ImportResult texture_source_importer(
    const ImportContext& context);

ImportResult model_source_importer(
    const ImportContext& context);

ImportResult audio_source_importer(
    const ImportContext& context);

} // namespace nengine::assets
