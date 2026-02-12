// Created by Unium on 12.02.26

#include "config.hpp"
#include "ipc.hpp"
#include "scheduler.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t g_sd = 0;
static void sigH(int s) {
  (void)s;
  g_sd = 1;
}

static void wPID() {
  std::string p = DConf::pidPth();
  std::ofstream f(p);
  if (f.is_open()) {
    f << getpid() << "\n";
  }
}
static void rPID() { unlink(DConf::pidPth().c_str()); }

static bool iaRun() {
  std::string p = DConf::pidPth();
  std::ifstream f(p);
  if (!f.is_open())
    return false;

  pid_t pid;
  f >> pid;
  if (pid <= 0)
    return false;

  if (kill(pid, 0) == 0) {
    return true;
  }
  unlink(p.c_str());
  return false;
}

static void spkDmns() {
  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "err: fork failed\n";
    exit(1);
  }
  if (pid > 0) {
    std::cout << "ndatmx started (pid " << pid << ")\n";
    exit(0);
  }

  setsid();

  pid = fork();
  if (pid < 0)
    exit(1);
  if (pid > 0)
    exit(0);

  close(STDIN_FILENO);
  open("/dev/null", O_RDONLY);

  DConf cfg;
  cfg.load();
  int lfd = open(cfg.logFile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (lfd >= 0) {
    dup2(lfd, STDOUT_FILENO);
    dup2(lfd, STDERR_FILENO);
    close(lfd);
  }

  umask(0022);
}

static void pUsg() {
  std::cout << "usage: ndatmxd [options]\n"
            << "options:\n"
            << "  -f, --foreground    runs in foreground instead of as a "
               "daemon\n"
            << "  -k, --kill          stop running daemons\n"
            << "  -s, --status        check is daemon is running\n"
            << "  -h, --help          show this menu\n";
}

int main(int argc, char **argv) {
  bool fg = false;
  bool dk = false;
  bool ds = false;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-f" || a == "--foreground")
      fg = true;
    else if (a == "-k" || a == "--kill")
      dk = true;
    else if (a == "-s" || a == "--status")
      ds = true;
    else if (a == "-h" || a == "--help") {
      pUsg();
      return 0;
    } else {
      std::cerr << "err: unknown option: " << a << "\n";
      pUsg();
      return 1;
    }
  }

  if (ds) {
    if (iaRun()) {
      std::ifstream f(DConf::pidPth());
      pid_t pid;
      f >> pid;
      std::cout << "ndatmxd is running (pid " << pid << ")\n";
      return 0;
    } else {
      std::cout << "ndatmxd is not running\n";
      return 1;
    }
  }

  if (dk) {
    std::ifstream f(DConf::pidPth());
    if (!f.is_open()) {
      std::cerr << "err: ndatmxd is not running (no pid file)\n";
      return 1;
    }
    pid_t pid;
    f >> pid;
    if (pid <= 0 || kill(pid, 0) != 0) {
      std::cerr << "err: ndatmxd is not running (stale pid)\n";
      rPID();
      return 1;
    }
    kill(pid, SIGTERM);
    std::cout << "sent SIGTERM to ndatmxd (pid " << pid << ")\n";
    for (int i = 0; i < 30; ++i) {
      usleep(100000);
      if (kill(pid, 0) != 0) {
        std::cout << "ndatmxd stopped\n";
        return 0;
      }
    }
    kill(pid, SIGKILL);
    std::cout << "ndatmxd killed\n";
    return 0;
  }

  if (iaRun()) {
    std::cerr << "err: ndatmxd is already running\n";
    return 1;
  }

  DConf iCfg;
  iCfg.load();
  iCfg.eDir();
  struct stat st;
  if (stat(DConf::confPth().c_str(), &st) != 0) {
    iCfg.save();
  }

  if (!fg)
    spkDmns();

  signal(SIGTERM, sigH);
  signal(SIGINT, sigH);
  signal(SIGHUP, sigH);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);

  wPID();

  Scheduler sch;
  sch.ldCfg();
  sch.start();

  ISvr ipc(sch);
  ipc.strt();

  std::cout << "ndatmxd running (pid " << getpid() << ")\n";

  while (!sch.sShtDn() && !g_sd) {
    sleep(1);
  }

  std::cout << "ndatmxd shutting down, bye bye!";
  ipc.stp();
  sch.stop();
  rPID();

  return 0;
}
