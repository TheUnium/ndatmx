// Created by Unium on 12.02.26

#include "mgit.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static bool dExists(const std::string &p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool fExists(const std::string &p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0;
}

static int cexec(const std::string &c, std::string *o) {
  if (o)
    o->clear();
  const char *tmp_env = std::getenv("TMPDIR");
  std::string tmp_dir = tmp_env ? tmp_env : "/tmp";
  std::string t = tmp_dir + "/ndatmx_cmd_" + std::to_string(getpid()) + ".out";
  std::string f = c + " > '" + t + "' 2>&1";

  int rc = system(f.c_str());
  if (o) {
    std::ifstream f(t);
    if (f.is_open()) {
      std::stringstream ss;
      ss << f.rdbuf();
      *o = ss.str();
    }
  }

  unlink(t.c_str());
  if (rc == -1)
    return -1;
  if (WIFEXITED(rc))
    return WEXITSTATUS(rc);
  return -1;
}

int mgit::rIn(const std::string &dir, const std::string &args, std::string *o) {
  std::string c = "cd '" + dir + "' && git " + args;
  return cexec(c, o);
};

bool mgit::clone(const std::string &url, const std::string &dp,
                 const std::string &branch) {
  if (dExists(dp + "/.git")) {
    return true;
  }

  std::string o;
  std::string c = "git clone --branch '" + branch + "' --single-branch '" +
                  url + "' '" + dp + "'";
  cexec(c, &o);

  if (dExists(dp + "/.git")) {
    return true;
  }

  if (dExists(dp)) {
    std::string rm_c = "rm -rf '" + dp + "'";
    cexec(rm_c, &o);
    usleep(100000);
  }

  c = "git clone '" + url + "' '" + dp + "'";
  cexec(c, &o);

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
