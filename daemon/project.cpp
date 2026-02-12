// Created by Unium on 12.02.26

#include "project.hpp"

std::string pStsS(PSts s) {
  switch (s) {
  case PSts::IDLE:
    return "idle";
  case PSts::CLON:
    return "cloning";
  case PSts::UPD:
    return "updating";
  case PSts::BLD:
    return "building";
  case PSts::RUN:
    return "running";
  case PSts::FAIL:
    return "failed";
  case PSts::STOP:
    return "stopped";
  case PSts::EXT_OK:
    return "exited (ok)";
  case PSts::DIS:
    return "disabled";
  }
  return "unknown";
}

std::string PStt::stsS() const { return pStsS(sts); }

std::string PStt::logP() const {
  if (cfg.lPTH.empty())
    return "";
  return cfg.lPTH + ".log";
}
