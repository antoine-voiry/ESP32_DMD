#ifndef DMD_CORE_SPECIAL_MOVES_H
#define DMD_CORE_SPECIAL_MOVES_H

// Port of Raspy2DMD DMDRenderer_SpecialsMoves.SearchSpecialsMoves().

#include <string>
#include <vector>

namespace dmd {

// darts: three upper-cased segments ("T20", "SB", ...), none of them "X".
// Returns the special move name ("MAXIMUM_TON_80", ...) or "" when there is none.
std::string findSpecialMove(const std::vector<std::string>& darts);

}  // namespace dmd

#endif
