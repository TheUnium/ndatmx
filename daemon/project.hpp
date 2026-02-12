// Created by Unium on 12.02.26

#pragma once

#include "config.hpp"
#include <chrono>
#include <string>
#include <sys/types.h>

enum class PSts { IDLE, CLON, UPD, BLD, RUN, FAIL, STOP, EXT_OK, DIS };

struct PStt {
  PEnt cfg;
  PSts sts = PSts::IDLE;
  std::string lCmt;
  std::string lErr;
  pid_t pid = -1;
  int lExt = -1;
  std::chrono::steady_clock::time_point lChk;
  std::chrono::steady_clock::time_point lRun;
  bool strtOnce = false;
  int flCnt = 0;

  std::string stsS() const;
  std::string logP() const;
};

std::string pStsS(PSts s);
