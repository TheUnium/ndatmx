// Created by Unium on 12.02.26

#pragma once

#include "scheduler.hpp"
#include <atomic>
#include <string>
#include <thread>

class ISvr {
public:
  ISvr(Scheduler &sch);
  ~ISvr();

  void strt();
  void stp();

private:
  Scheduler &sch;
  int fd = -1;
  std::atomic<bool> run{false};
  std::thread th;

  void loop();
  void hCl(int c);
  std::string dsp(const std::string &c);
};
