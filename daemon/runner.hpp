// Created by Unium on 12.02.26

#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

namespace rnr {
bool rCmds(const std::vector<std::string> &cmds, const std::string &wd,
           const std::string &lf);
pid_t rCmdsBg(const std::vector<std::string> &cmds, const std::string &wd,
              const std::string &lf);
bool kPrc(pid_t pid);
bool isRun(pid_t pid);
bool chkExt(pid_t pid, int &ec);
} // namespace rnr
