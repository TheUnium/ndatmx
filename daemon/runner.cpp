// Created by Unium on 12.02.26

#include "runner.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

bool rnr::rCmds(const std::vector<std::string> &cmds, const std::string &wd,
                const std::string &lf) {
  std::string s = "set -e\n";
  s += "cd '" + wd + "'\n";
  for (const auto &c : cmds) {
    s += c + "\n";
  }

  std::string sp = "/tmp/ndatmx_run_" + std::to_string(getpid()) + ".sh";
  {
    std::ofstream f(sp);
    f << s;
  }

  std::string fc = "bash '" + sp + "'";
  if (!lf.empty()) {
    fc += " >> '" + lf + "' 2>&1";
  }

  int rc = system(fc.c_str());
  unlink(sp.c_str());
  return WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

pid_t rnr::rCmdsBg(const std::vector<std::string> &cmds, const std::string &wd,
                   const std::string &lf) {
  std::string s = "set -e\n";
  s += "cd '" + wd + "'\n";
  for (const auto &c : cmds) {
    s += c + "\n";
  }

  static int cnt = 0;
  std::string sp = "/tmp/ndatmx_bg_" + std::to_string(getpid()) + "_" +
                   std::to_string(cnt++) + ".sh";
  {
    std::ofstream f(sp);
    f << s;
  }

  pid_t pid = fork();
  if (pid < 0) {
    unlink(sp.c_str());
    return -1;
  }

  if (pid == 0) {
    setsid();
    if (!lf.empty()) {
      int fd = open(lf.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (fd >= 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
    }
    signal(SIGCHLD, SIG_DFL);
    execl("/bin/bash", "bash", sp.c_str(), nullptr);
    _exit(127);
  }
  return pid;
}

bool rnr::kPrc(pid_t pid) {
  if (pid <= 0)
    return false;

  if (kill(pid, SIGTERM) != 0) {
    if (errno == ESRCH)
      return true;
    return false;
  }

  for (int i = 0; i < 10; ++i) {
    usleep(200000);
    int st;
    pid_t r = waitpid(pid, &st, WNOHANG);
    if (r == pid)
      return true;
    if (kill(pid, 0) != 0)
      return true;
  }

  kill(pid, SIGKILL);
  waitpid(pid, nullptr, 0);
  return true;
}

bool rnr::isRun(pid_t pid) {
  if (pid <= 0)
    return false;
  int st;
  pid_t r = waitpid(pid, &st, WNOHANG);
  if (r == pid)
    return false;
  if (r == 0)
    return true;
  return kill(pid, 0) == 0;
}

bool rnr::chkExt(pid_t pid, int &ec) {
  if (pid <= 0) {
    ec = -1;
    return true;
  }
  int st;
  pid_t r = waitpid(pid, &st, WNOHANG);
  if (r == pid) {
    if (WIFEXITED(st)) {
      ec = WEXITSTATUS(st);
    } else if (WIFSIGNALED(st)) {
      ec = 128 + WTERMSIG(st);
    } else {
      ec = -1;
    }
    return true;
  }
  if (r == 0)
    return false;

  if (errno == ECHILD) {
    if (kill(pid, 0) != 0) {
      ec = 0;
      return true;
    }
    return false;
  }
  return false;
}
