#pragma once
#include "native_widget.h"
#include "estrlist.h"
#include <array>
#include <string>
namespace olex2::harmony::ui {
struct GenerationDraft {std::array<std::string,3> from{{"-1","-1","-1"}};std::array<std::string,3> to{{"1","1","1"}};std::array<std::string,3> range{{"2","2","2"}};};
struct GenerationValues {std::array<float,3> from{{-1,-1,-1}};std::array<float,3> to{{1,1,1}};};
// Preserve wxString::ToDouble failure behavior: an invalid value leaves the
// caller-provided previous value unchanged. The caller controls value order.
double ParseOriginalGenerationValue(const olxstr&, double previous);
// Match the original sequential result conversion across A/B/C lower then
// A/B/C upper values using one reusable value initialized to 2.
GenerationValues ParseOriginalGenerationResult(const GenerationDraft&);
}
