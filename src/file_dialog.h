#pragma once

#include <optional>
#include <string>

// Opens a native file picker for glTF / GLB (Windows: common dialog; Linux:
// zenity, else kdialog). Reopens in the parent folder of the last successful
// pick when that folder still exists. Returns a path relative to the current
// working directory when possible, otherwise absolute.
std::optional<std::string> open_model_file_dialog();
