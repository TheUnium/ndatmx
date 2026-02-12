// Created by Unium on 12.02.26

#pragma once

#include <string>

enum class Plt { TRMX, DEB13, DEB12, DEB11, ARCH, FED, LIN, UNK };

namespace plt {
Plt dtct();
std::string toS(Plt p);
Plt frS(const std::string &s);
bool mtch(Plt d, const std::string &s);
} // namespace plt
