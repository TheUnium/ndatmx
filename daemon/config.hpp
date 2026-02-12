// Created by Unium on 12.02.06

#pragma once

#include <string>
#include <vector>

struct PEnt {
  std::string n;
  std::string gURL;
  std::string lPTH;
  std::string brnc = "main";
  bool enabled = true;
  int pllIntvlMin = 0; // 0 = use global conf
};

struct DConf {
  int dPllIntvlMin = 10;
  int logRetDays = 7;
  std::string prjDir;
  std::string logFile;
  std::string logArcvDir;
  std::vector<PEnt> projs;

  static std::string confDir();
  static std::string confPth();
  static std::string sockPth();
  static std::string pidPth();

  void load();
  void save() const;
  void eDir() const;

  PEnt *fProjs(const std::string &n);
  const PEnt *fProjs(const std::string &n) const; // we love const overloading
};
