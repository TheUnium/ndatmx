// Created by Unium on 12.02.26

#include "ndxparser.hpp"
#include "platform.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

static std::string trim(const std::string &s) {
  size_t st = s.find_first_not_of(" \t\r\n");
  if (st == std::string::npos)
    return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(st, e - st + 1);
}

static std::string unq(const std::string &s) {
  std::string r = s;
  if (r.size() >= 2 && r.front() == '"' && r.back() == '"') {
    r = r.substr(1, r.size() - 2);
  }
  std::string o;
  for (size_t i = 0; i < r.size(); ++i) {
    if (r[i] == '\\' && i + 1 < r.size() && r[i + 1] == '"') {
      o += '"';
      ++i;
    } else {
      o += r[i];
    }
  }
  return o;
}

static std::string sLwr(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

NFle NFle::prs(const std::string &fp) {
  NFle n;
  std::ifstream f(fp);
  if (!f.is_open()) {
    std::cerr << "err: cannot open: " << fp << "\n";
    return n;
  }

  enum class Sec { NONE, DMN, CMDS };
  Sec cSec = Sec::NONE;

  std::string l;
  NCmds cCmds;
  bool inCmds = false;
  std::map<int, std::string> nCmds;

  auto flshCmds = [&]() {
    if (inCmds && !cCmds.pltSpec.empty()) {
      for (auto &[nm, cmd] : nCmds) {
        cCmds.cmds.push_back(cmd);
      }
      n.cmdBlks.push_back(cCmds);
    }
    cCmds = NCmds{};
    nCmds.clear();
    inCmds = false;
  };

  while (std::getline(f, l)) {
    l = trim(l);
    if (l.empty() || l[0] == '#')
      continue;

    if (l.front() == '[' && l.back() == ']') {
      flshCmds();
      std::string s = l.substr(1, l.size() - 2);
      std::string sl = sLwr(s);

      if (sl == "daemon") {
        cSec = Sec::DMN;
        n.dSet.hasSet = true;
      } else if (s.find("commands:") == 0) {
        cSec = Sec::CMDS;
        cCmds.pltSpec = s.substr(9);
        inCmds = true;
      } else {
        cSec = Sec::NONE;
      }
      continue;
    }

    auto eq = l.find('=');
    if (eq == std::string::npos)
      continue;

    std::string k = sLwr(trim(l.substr(0, eq)));
    std::string v = unq(trim(l.substr(eq + 1)));
    std::string vl = sLwr(v);

    if (cSec == Sec::DMN) {
      if (k == "poll_interval") {
        try {
          n.dSet.pllIntvl = std::stoi(v);
        } catch (...) {
        }
      } else if (k == "log_retention" || k == "log_retention_days") {
        try {
          n.dSet.logRetDays = std::stoi(v);
        } catch (...) {
        }
      } else if (k == "restart_on_exit" || k == "auto_restart") {
        n.dSet.rstrtOnExit = (vl == "true" || v == "1");
      } else if (k == "restart_on_success") {
        n.dSet.rstrtOnSucc = (vl == "true" || v == "1");
      } else if (k == "max_retries") {
        try {
          n.dSet.maxRetries = std::stoi(v);
        } catch (...) {
        }
      } else if (k == "branch") {
        n.dSet.brnc = v;
      }
    } else if (cSec == Sec::CMDS && inCmds) {
      try {
        int num = std::stoi(k);
        nCmds[num] = v;
      } catch (...) {
        nCmds[(int)nCmds.size() + 1000] = v;
      }
    }
  }

  flshCmds();
  return n;
}

const NCmds *NFle::mPlt() const {
  Plt cur = plt::dtct();

  for (const auto &b : cmdBlks) {
    if (plt::mtch(cur, b.pltSpec)) {
      Plt sp = plt::frS(b.pltSpec);
      if (sp == cur)
        return &b;
    }
  }

  for (const auto &b : cmdBlks) {
    if (plt::mtch(cur, b.pltSpec)) {
      return &b;
    }
  }

  return nullptr;
}
