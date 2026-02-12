// Created by Unium on 12.02.26

#include "mgit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

static bool dExists(const std::string &p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static int cexec_d(const std::vector<std::string> &args,
                   const std::string &wd = "", std::string *o = nullptr) {
  if (o)
    o->clear();
  if (args.empty())
    return -1;

  const char *te = std::getenv("TMPDIR");
  std::string td = te ? te : "/tmp";
  std::string tf = td + "/ndatmx_cmd_" + std::to_string(getpid()) + "_" +
                   std::to_string(rand()) + ".out";

  pid_t pid = fork();
  if (pid < 0)
    return -1;

  if (pid == 0) {
    if (!wd.empty()) {
      if (chdir(wd.c_str()) != 0)
        _exit(127);
    }

    int fd = open(tf.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      close(fd);
    }

    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);

    std::vector<char *> av;
    for (auto &a : args)
      av.push_back(const_cast<char *>(a.c_str()));
    av.push_back(nullptr);

    execvp(av[0], av.data());
    _exit(127);
  }

  int st = 0;
  pid_t r;
  do {
    r = waitpid(pid, &st, 0);
  } while (r == -1 && errno == EINTR);

  if (o) {
    std::ifstream f(tf);
    if (f.is_open()) {
      std::stringstream ss;
      ss << f.rdbuf();
      *o = ss.str();
    }
  }
  unlink(tf.c_str());

  if (r == -1)
    return -1;
  if (WIFEXITED(st))
    return WEXITSTATUS(st);
  return -1;
}

int mgit::rIn(const std::string &dir, const std::string &args, std::string *o) {
  std::vector<std::string> av;
  av.push_back("git");
  std::istringstream iss(args);
  std::string tk;
  while (iss >> tk)
    av.push_back(tk);

  return cexec_d(av, dir, o);
}

bool mgit::clone(const std::string &url, const std::string &dp,
                 const std::string &branch) {
  if (dExists(dp + "/.git")) {
    return true;
  }

  std::string o;
  cexec_d({"git", "clone", "--branch", branch, "--single-branch", url, dp}, "",
          &o);

  if (dExists(dp + "/.git")) {
    return true;
  }

  if (dExists(dp)) {
    cexec_d({"rm", "-rf", dp});
    usleep(100000);
  }

  cexec_d({"git", "clone", url, dp}, "", &o);

  if (dExists(dp + "/.git")) {
    return true;
  }

  fprintf(stderr, "err: git > clone failed: %s\n", o.c_str());
  return false;
}

bool mgit::hUpdates(const std::string &rp, const std::string &branch) {
  if (!dExists(rp + "/.git"))
    return false;

  std::string o;
  int rc = rIn(rp, "fetch origin", &o);
  if (rc != 0)
    return false;

  std::string lh, rh;
  rIn(rp, "rev-parse HEAD", &lh);

  rc = rIn(rp, "rev-parse origin/" + branch, &rh);
  if (rc != 0) {
    std::string ref;
    rIn(rp, "symbolic-ref refs/remotes/origin/HEAD", &ref);
    while (!ref.empty() && (ref.back() == '\n' || ref.back() == '\r'))
      ref.pop_back();
    if (!ref.empty()) {
      rIn(rp, "rev-parse " + ref, &rh);
    }
    if (rh.empty()) {
      rc = rIn(rp, "rev-parse origin/main", &rh);
      if (rc != 0) {
        rIn(rp, "rev-parse origin/master", &rh);
      }
    }
  }

  while (!lh.empty() && (lh.back() == '\n' || lh.back() == '\r'))
    lh.pop_back();
  while (!rh.empty() && (rh.back() == '\n' || rh.back() == '\r'))
    rh.pop_back();

  if (rh.empty())
    return false;
  return lh != rh;
}

bool mgit::pull(const std::string &rp) {
  std::string b;
  rIn(rp, "rev-parse HEAD", &b);

  std::string o;
  int rc = rIn(rp, "pull --ff-only", &o);
  if (rc != 0) {
    rc = rIn(rp, "pull --rebase", &o);
    if (rc != 0) {
      // rIn(rp, "reset --hard origin/HEAD", &o);
      fprintf(stderr, "err: git > pull failed %s\n", o.c_str());
      return false;
    }
  }

  std::string a;
  rIn(rp, "rev-parse HEAD", &a);

  while (!b.empty() && (b.back() == '\n' || b.back() == '\r'))
    b.pop_back();
  while (!a.empty() && (a.back() == '\n' || a.back() == '\r'))
    a.pop_back();

  return b != a;
}

std::string mgit::cCommit(const std::string &rp) {
  std::string o;
  rIn(rp, "rev-parse --short HEAD", &o);
  while (!o.empty() && (o.back() == '\n' || o.back() == '\r'))
    o.pop_back();
  return o;
}
