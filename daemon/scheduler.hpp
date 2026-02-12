// Created by Unium on 12.02.26

#pragma once

#include "config.hpp"
#include "project.hpp"
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class Scheduler {
public:
  Scheduler();
  ~Scheduler();

  void ldCfg();
  void rldCfg();
  void start();
  void stop();
  bool sShtDn() const { return shtDn.load(); }

  std::string aProj(const std::string &n, const std::string &gURL,
                     const std::string &brnc);
  std::string rProj(const std::string &n);
  std::string eProj(const std::string &n);
  std::string dProj(const std::string &n);
  std::string rsProj(const std::string &n);
  std::string sProj(const std::string &n);
  std::string prjSts(const std::string &n);
  std::string lstPrj();
  std::string getLog(const std::string &n, int lns);

  std::string sPllInt(int min);
  std::string sLogRet(int dys);
  std::string getCfg();

  std::string reqShtDn();

private:
  DConf cfg;
  std::map<std::string, PStt> stts;
  std::mutex mtx;
  std::atomic<bool> run{false};
  std::atomic<bool> shtDn{false};
  std::atomic<bool> woke{false};
  std::thread thrd;

  std::mutex wMtx;
  std::condition_variable wCv;

  std::chrono::steady_clock::time_point lGc;
  std::chrono::steady_clock::time_point lLogCln;

  void loop();
  void chkProj(PStt &st);
  void runProj(PStt &st);
  void stpPrjInt(PStt &st);
  void arcLog(const std::string &lp,
              const std::string &pn);
  void clnLog();
  void runGc();
  void svCfg();
  void log(const std::string &msg);
  void wake();
  void sOw(int s);
};
