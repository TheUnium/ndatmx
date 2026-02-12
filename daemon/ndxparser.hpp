// Created by Unium on 12.02.26

#pragma once

#include <string>
#include <vector>

struct NDSet {
  int pllIntvl = -1;
  int logRetDays = -1;
  bool rstrtOnExit = true;
  bool rstrtOnSucc = false;
  int maxRetries = -1;
  std::string brnc;
  bool hasSet = false;
};

struct NCmds {
  std::string pltSpec;
  std::vector<std::string> cmds;
};

struct NFle {
  NDSet dSet;
  std::vector<NCmds> cmdBlks;

  static NFle prs(const std::string &fp);
  const NCmds *mPlt() const;
};
