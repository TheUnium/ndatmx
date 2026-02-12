// Created by Unium on 12.02.26

#include "runner.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static std::string gTmp() {
  const char *t = std::getenv("TMPDIR");
  if (t)
    return t;
  const char *p = std::getenv("PREFIX");
  if (p)
    return std::string(p) + "/tmp";
  return "/tmp";
}

static std::string gBash() {
  const char *p = std::getenv("PREFIX");
  if (p) {
    std::string bp = std::string(p) + "/bin/bash";
    struct stat st;
    if (stat(bp.c_str(), &st) == 0)
      return bp;
  }
  return "bash";
}

bool rnr::rCmds(const std::vector<std::string> &cmds, const std::string &wd,
                const std::string &lf) {
  std::string s = "set -e\n";
  s += "cd '" + wd + "'\n";
  for (const auto &c : cmds) {
    s += c + "\n";
  }

  std::string td = gTmp();
  std::string sp = td + "/ndatmx_run_" + std::to_string(getpid()) + ".sh";
  {
    std::ofstream f(sp);
    f << s;
  }

  std::string bash = gBash();

  pid_t pid = fork();
  if (pid < 0) {
    unlink(sp.c_str());
    return false;
  }

  if (pid == 0) {
    if (!lf.empty()) {
      int fd = open(lf.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (fd >= 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
    }
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    execl(bash.c_str(), "bash", sp.c_str(), nullptr);
    execlp("bash", "bash", sp.c_str(), nullptr);
    _exit(127);
  }

  int st = 0;
  pid_t r;
  do {
    r = waitpid(pid, &st, 0);
  } while (r == -1 && errno == EINTR);

  unlink(sp.c_str());

  if (r == -1)
    return false;
  return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

pid_t rnr::rCmdsBg(const std::vector<std::string> &cmds, const std::string &wd,
                   const std::string &lf) {
  std::string s = "set -e\n";
  s += "cd '" + wd + "'\n";
  for (const auto &c : cmds) {
    s += c + "\n";
  }

  static int cnt = 0;
  std::string td = gTmp();
  std::string sp = td + "/ndatmx_bg_" + std::to_string(getpid()) + "_" +
                   std::to_string(cnt++) + ".sh";
  {
    std::ofstream f(sp);
    f << s;
  }

  std::string bash = gBash();

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
    signal(SIGPIPE, SIG_DFL);
    execl(bash.c_str(), "bash", sp.c_str(), nullptr);
    execlp("bash", "bash", sp.c_str(), nullptr);
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
