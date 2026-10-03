#include "SpecialMoves.h"

#include <algorithm>
#include <cstdlib>

namespace dmd {

namespace {

int dartValue(const std::string& dart) {
    if (dart == "SB") return 25;
    if (dart == "DB") return 50;
    if (dart.size() < 2) return 0;
    int n = std::atoi(dart.c_str() + 1);
    switch (dart[0]) {
        case 'S': return n;
        case 'D': return n * 2;
        case 'T': return n * 3;
        default: return 0;
    }
}

bool isBull(const std::string& dart) {
    return dart == "SB" || dart == "DB";
}

// True if the three darts are exactly {a, b, c} in any order.
bool isPermutationOf(const std::vector<std::string>& darts, const char* a, const char* b, const char* c) {
    std::vector<std::string> got = darts;
    std::vector<std::string> want = {a, b, c};
    std::sort(got.begin(), got.end());
    std::sort(want.begin(), want.end());
    return got == want;
}

}  // namespace

std::string findSpecialMove(const std::vector<std::string>& darts) {
    // The original indexes valeurs[0..2] directly; anything other than 3 darts raised there.
    if (darts.size() != 3) {
        return "";
    }
    const std::string& d1 = darts[0];
    const std::string& d2 = darts[1];
    const std::string& d3 = darts[2];

    int total = 0;
    for (const auto& d : darts) {
        total += dartValue(d);
    }

    // "A la touche" (by segment), in the original order.
    if (d1 == "DB" && d2 == "DB" && d3 == "DB") return "BLACK_HAT_THREE_IN_THE_BLACK";
    if (d1 == "SB" && d2 == "SB" && d3 == "SB") return "RED_HAT";
    if (isBull(d1) && isBull(d2) && isBull(d3)) return "HAT_TRICK";
    if (d1 == "T20" && d2 == "T20" && d3 == "T20") return "MAXIMUM_TON_80";
    if (d1 == "T1" && d2 == "T1" && d3 == "T1") return "ROUND_OF_TERMS";
    if (d1 == "S6" && d2 == "S6" && d3 == "S6") return "DEVIL";
    if (d1 == "S1" && d2 == "S1" && d3 == "S1") return "BUCKET_OF_NAIL";
    if (isPermutationOf(darts, "S1", "S5", "S20")) return "BREAKFAST";
    if (isPermutationOf(darts, "T1", "T5", "T20")) return "CHAMPAGNE_BREAKFAST";
    if (isPermutationOf(darts, "S12", "S5", "S20")) return "NOT_OLD";
    if (d1 == d2 && d2 == d3) return "THREE_IN_A_BED";

    // "Au total".
    if (total == 0) return "WOODY";
    if (total > 0 && total <= 10) return "CIRCLE_IT";
    if (total == 22) return "DINKY_DOO";
    if (total == 33) return "FEATHERS";
    if (total == 45) return "BAG_O_NUT";
    if (total == 57) return "VARIETIES";
    if (total == 60) return "STEADY";
    if (total == 66) return "ROUTE_66";
    if (total == 69) return "HAPPY_MEAL";
    if (total == 76) return "TROMBONES";
    if (total == 77) return "SUNSET_STRIP";
    if (total == 88) return "GARDEN_GATE_FAT_LADIES";
    if (total == 95) return "BABY_TON";
    if (total >= 100 && total <= 150) return "LOW_TON";
    if (total >= 151 && total <= 179) return "HIGH_TON";
    return "";
}

}  // namespace dmd
