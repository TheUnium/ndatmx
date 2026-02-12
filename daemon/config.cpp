// Created by Unium on 12.02.26

#include "config.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

static std::string gHome() {
  const char *h = std::getenv("HOME");
  return h ? std::string(h) : "/tmp";
}

static void cmkdir(const std::string &p) {
  std::string t;
  for (char c : p) {
    t += c;
    if (c == '/') {
      mkdir(t.c_str(), 0755);
    }
  }
  mkdir(t.c_str(), 0755);
}

static std::string trim(const std::string &s) {
  size_t st = s.find_first_not_of(" \t\r\n");
  if (st == std::string::npos)
    return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(st, e - st + 1);
}

static std::string unq(const std::string &s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

std::string DConf::confDir() { return gHome() + "/.config/ndatmx"; }
std::string DConf::confPth() { return confDir() + "/config.toml"; }
std::string DConf::pidPth() { return confDir() + "/ndatmx.pid"; }
std::string DConf::sockPth() {
  const char *t = std::getenv("TMPDIR");
  std::string b = t ? std::string(t) : "/tmp";
  return b + "/ndatmx.sock";
}

void DConf::eDir() const {
  cmkdir(confDir());
  if (!prjDir.empty()) {
    cmkdir(prjDir);
  }
  if (!logArcvDir.empty()) {
    cmkdir(logArcvDir);
  }
}

void DConf::load() {
  prjDir = gHome() + "/ndatmx_projects";
  logFile = confDir() + "/daemon.log";
  logArcvDir = prjDir + "/.logs";

  std::ifstream f(confPth());
  if (!f.is_open())
    return;

  std::string l;
  std::string cSec;
  std::string cPrjName;
  PEnt cPrj;
  bool inPrj = false;

  auto flshPrj = [&]() {
    if (inPrj && !cPrj.n.empty()) {
      auto *ex = fProjs(cPrj.n);
      if (ex) {
        *ex = cPrj;
      } else {
        projs.push_back(cPrj);
      }
    }
    cPrj = PEnt{};
    inPrj = false;
  };

  while (std::getline(f, l)) {
    l = trim(l);
    if (l.empty() || l[0] == '#')
      continue;

    if (l.front() == '[' && l.back() == ']') {
      flshPrj();
      cSec = l.substr(1, l.size() - 2);
      if (cSec.find("project.") == 0) {
        cPrjName = cSec.substr(8);
        cPrj = PEnt{};
        cPrj.n = cPrjName;
        inPrj = true;
      }
      continue;
    }

    auto eq = l.find('=');
    if (eq == std::string::npos)
      continue;

    std::string k = trim(l.substr(0, eq));
    std::string v = unq(trim(l.substr(eq + 1)));

    if (cSec == "daemon") {
      if (k == "poll_interval")
        dPllIntvlMin = std::stoi(v);
      else if (k == "projects_dir")
        prjDir = v;
      else if (k == "log_file")
        logFile = v;
      else if (k == "log_archive_dir")
        logArcvDir = v;
      else if (k == "log_retention_days")
        logRetDays = std::stoi(v);
    } else if (inPrj) {
      if (k == "git")
        cPrj.gURL = v;
      else if (k == "path")
        cPrj.lPTH = v;
      else if (k == "branch")
        cPrj.brnc = v;
      else if (k == "enabled")
        cPrj.enabled = (v == "true" || v == "1");
      else if (k == "poll_interval")
        cPrj.pllIntvlMin = std::stoi(v);
    }
  }

  flshPrj();
  for (auto &p : projs) {
    if (p.lPTH.empty()) {
      p.lPTH = prjDir + "/" + p.n;
    }
  }
}

void DConf::save() const {
  eDir();
  std::ofstream f(confPth());
  if (!f.is_open()) {
    std::cerr << "err: cannot write to " << confPth() << "\n";
    return;
  }

  f << "# ndatmx daemon configuration\n\n";
  f << "[daemon]\n";
  f << "poll_interval = " << dPllIntvlMin << "\n";
  f << "projects_dir = \"" << prjDir << "\"\n";
  f << "log_file = \"" << logFile << "\"\n";
  f << "log_archive_dir = \"" << logArcvDir << "\"\n";
  f << "log_retention_days = \"" << logRetDays << "\"\n";
  f << "\n";

  for (const auto &p : projs) {
    f << "[project." << p.n << "]\n";
    f << "git = \"" << p.gURL << "\"\n";
    if (!p.lPTH.empty()) {
      f << "path = \"" << p.lPTH << "\"\n";
    }
    f << "branch = \"" << p.brnc << "\"\n";
    f << "enabled = " << (p.enabled ? "true" : "false") << "\n";
    if (p.pllIntvlMin > 0) {
      f << "poll_interval = " << p.pllIntvlMin << "\n";
    }
    f << "\n";
  }
}

PEnt *DConf::fProjs(const std::string &n) {
  for (auto &p : projs) {
    if (p.n == n)
      return &p;
  }
  return nullptr;
}

const PEnt *DConf::fProjs(const std::string &n) const {
  for (const auto &p : projs) {
    if (p.n == n)
      return &p;
  }
  return nullptr;
}
