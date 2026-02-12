// Created by Unium on 12.02.26

#include "platform.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

static std::string sLwr(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

static std::string rdFile(const std::string &p) {
  std::ifstream f(p);
  if (!f.is_open())
    return "";
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static std::string gRelFld(const std::string &fld) {
  std::ifstream f("/etc/os-release");
  if (!f.is_open())
    return "";
  std::string l;
  while (std::getline(f, l)) {
    if (l.find(fld + "=") == 0) {
      std::string v = l.substr(fld.size() + 1);
      if (!v.empty() && v.front() == '"')
        v.erase(0, 1);
      if (!v.empty() && v.back() == '"')
        v.pop_back();
      return v;
    }
  }
  return "";
}

Plt plt::dtct() {
  const char *p = std::getenv("PREFIX");
  if (p) {
    if (std::string(p).find("com.termux") != std::string::npos) {
      return Plt::TRMX;
    }
  }
  if (std::getenv("TERMUX_VERSION")) {
    return Plt::TRMX;
  }

  std::string id = sLwr(gRelFld("ID"));
  std::string v = gRelFld("VERSION_ID");

  if (id == "debian") {
    if (v.find("13") == 0)
      return Plt::DEB13;
    if (v.find("12") == 0)
      return Plt::DEB12;
    if (v.find("11") == 0)
      return Plt::DEB11;
    return Plt::LIN;
  }
  if (id == "arch")
    return Plt::ARCH;
  if (id == "fedora")
    return Plt::FED;

  if (!id.empty())
    return Plt::LIN;
  return Plt::UNK;
}

std::string plt::toS(Plt p) {
  switch (p) {
  case Plt::TRMX:
    return "termux";
  case Plt::DEB13:
    return "debian13";
  case Plt::DEB12:
    return "debian12";
  case Plt::DEB11:
    return "debian11";
  case Plt::ARCH:
    return "arch";
  case Plt::FED:
    return "fedora";
  case Plt::LIN:
    return "linux";
  case Plt::UNK:
    return "unknown";
  }
  return "unknown";
}

Plt plt::frS(const std::string &s) {
  std::string l = sLwr(s);
  if (l == "termux")
    return Plt::TRMX;
  if (l == "debian13")
    return Plt::DEB13;
  if (l == "debian12")
    return Plt::DEB12;
  if (l == "debian11")
    return Plt::DEB11;
  if (l == "arch")
    return Plt::ARCH;
  if (l == "fedora")
    return Plt::FED;
  if (l == "linux")
    return Plt::LIN;
  return Plt::UNK;
}

bool plt::mtch(Plt d, const std::string &s) {
  std::string l = sLwr(s);
  if (l == "any" || l == "all")
    return true;
  if (l == "linux" && d != Plt::UNK)
    return true;
  return d == frS(l);
}
