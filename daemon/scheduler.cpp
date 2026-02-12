// Created by Unium on 12.02.26

#include "scheduler.hpp"
#include "mgit.hpp"
#include "ndxparser.hpp"
#include "platform.hpp"
#include "runner.hpp"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static void cmkdir(const std::string &p) {
  std::string tmp;
  for (char c : p) {
    tmp += c;
    if (c == '/')
      mkdir(tmp.c_str(), 0755);
  }
  mkdir(tmp.c_str(), 0755);
}

static std::string tFile(const std::string &p, int ls) {
  std::ifstream f(p);
  if (!f.is_open())
    return "(no log file)\n";

  std::vector<std::string> all;
  std::string l;
  while (std::getline(f, l)) {
    all.push_back(l);
  }

  std::string res;
  int start = (int)all.size() - ls;
  if (start < 0)
    start = 0;
  for (int i = start; i < (int)all.size(); ++i) {
    res += all[i] + "\n";
  }
  return res;
}

static std::string tsStr() {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&t));
  return std::string(buf);
}

static NDSet rSet(const std::string &lPath) {
  NDSet s;
  std::string xp = lPath + "/ndx.toml";
  struct stat st;
  if (stat(xp.c_str(), &st) == 0) {
    NFle x = NFle::prs(xp);
    s = x.dSet;
  }
  return s;
}

static int epi(const PStt &st, const NDSet &x, int gDef) {
  if (x.pllIntvl > 0)
    return x.pllIntvl;
  if (st.cfg.pllIntvlMin > 0)
    return st.cfg.pllIntvlMin;
  return gDef;
}

static int emr(const NDSet &x) {
  if (x.maxRetries >= 0)
    return x.maxRetries;
  return 3;
}

Scheduler::Scheduler() {
  lGc = std::chrono::steady_clock::now();
  lLogCln = std::chrono::steady_clock::now();
}

Scheduler::~Scheduler() { stop(); }

void Scheduler::wake() {
  woke = true;
  std::lock_guard<std::mutex> lk(wMtx);
  wCv.notify_all();
}

void Scheduler::sOw(int s) {
  std::unique_lock<std::mutex> lk(wMtx);
  wCv.wait_for(lk, std::chrono::seconds(s),
               [this] { return woke.load() || !run.load(); });
  woke = false;
}

void Scheduler::ldCfg() {
  std::lock_guard<std::mutex> lk(mtx);
  cfg.load();
  cfg.eDir();

  for (const auto &p : cfg.projs) {
    if (stts.find(p.n) == stts.end()) {
      PStt s;
      s.cfg = p;
      s.sts = p.enabled ? PSts::IDLE : PSts::DIS;
      stts[p.n] = s;
    } else {
      stts[p.n].cfg = p;
    }
  }
}

void Scheduler::rldCfg() {
  ldCfg();
  log("config reloaded");
  wake();
}

void Scheduler::log(const std::string &msg) {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));

  std::string full = std::string("[") + buf + "] " + msg + "\n";
  std::cerr << full;

  if (!cfg.logFile.empty()) {
    std::ofstream f(cfg.logFile, std::ios::app);
    if (f.is_open())
      f << full;
  }
}

void Scheduler::start() {
  run = true;
  shtDn = false;
  woke = false;
  thrd = std::thread(&Scheduler::loop, this);
  log("scheduler started, platform: " + plt::toS(plt::dtct()));
}

void Scheduler::stop() {
  run = false;
  wake();
  if (thrd.joinable()) {
    thrd.join();
  }

  std::lock_guard<std::mutex> lk(mtx);
  for (auto &[n, s] : stts) {
    if (s.sts == PSts::RUN) {
      stpPrjInt(s);
    }
  }
  log("scheduler stopped");
}

void Scheduler::loop() {
  while (run) {
    {
      std::lock_guard<std::mutex> lk(mtx);
      for (auto &[n, s] : stts) {
        if (!s.cfg.enabled) {
          if (s.sts == PSts::RUN) {
            stpPrjInt(s);
          }
          s.sts = PSts::DIS;
          continue;
        }

        if (s.sts == PSts::RUN) {
          int ec = -1;
          if (rnr::chkExt(s.pid, ec)) {
            s.pid = -1;
            s.lExt = ec;

            if (ec == 0) {
              log("[" + n + "] process exited normally (code 0)");
              s.sts = PSts::EXT_OK;
            } else {
              log("[" + n + "] process exited with code " + std::to_string(ec));
              s.sts = PSts::FAIL;
              s.lErr = "exited with code " + std::to_string(ec);
              s.flCnt++;
            }
          }
        }
        chkProj(s);
      }

      auto now = std::chrono::steady_clock::now();
      auto gce =
          std::chrono::duration_cast<std::chrono::hours>(now - lGc).count();
      if (gce >= 24) {
        runGc();
        lGc = now;
      }

      auto cle =
          std::chrono::duration_cast<std::chrono::hours>(now - lLogCln).count();
      if (cle >= 1) {
        clnLog();
        lLogCln = now;
      }
    }
    sOw(30);
  }
}

void Scheduler::chkProj(PStt &s) {
  auto now = std::chrono::steady_clock::now();
  std::string n = s.cfg.n;

  NDSet x = rSet(s.cfg.lPTH);

  if (!x.brnc.empty() && x.brnc != s.cfg.brnc) {
    log("[" + n + "] ndx.toml overrides branch to '" + x.brnc + "'");
    s.cfg.brnc = x.brnc;
  }

  int intvl = epi(s, x, cfg.dPllIntvlMin);

  auto elp =
      std::chrono::duration_cast<std::chrono::minutes>(now - s.lChk).count();

  if (!s.strtOnce && s.sts == PSts::IDLE) {
    // 1st rum
  } else if (elp < intvl) {
    return;
  }

  s.lChk = now;

  struct stat st;
  bool cln = stat((s.cfg.lPTH + "/.git").c_str(), &st) == 0;

  if (!cln) {
    log("[" + n + "] cloning from " + s.cfg.gURL);
    s.sts = PSts::CLON;
    cmkdir(cfg.prjDir);

    if (!mgit::clone(s.cfg.gURL, s.cfg.lPTH, s.cfg.brnc)) {
      log("[" + n + "] clone failed");
      s.sts = PSts::FAIL;
      s.lErr = "clone failed";
      s.flCnt++;
      return;
    }

    log("[" + n + "] cloned successfully");
    s.lCmt = mgit::cCommit(s.cfg.lPTH);

    x = rSet(s.cfg.lPTH);
    if (!x.brnc.empty()) {
      s.cfg.brnc = x.brnc;
    }

    runProj(s);
    return;
  }

  bool hUpd = mgit::hUpdates(s.cfg.lPTH, s.cfg.brnc);

  if (hUpd) {
    log("[" + n + "] updates found, pulling...");
    s.sts = PSts::UPD;

    if (s.pid > 0 && rnr::isRun(s.pid)) {
      log("[" + n + "] stopping for update...");
      stpPrjInt(s);
    }

    if (mgit::pull(s.cfg.lPTH)) {
      s.lCmt = mgit::cCommit(s.cfg.lPTH);
      log("[" + n + "] updated to " + s.lCmt);

      x = rSet(s.cfg.lPTH);
      if (!x.brnc.empty()) {
        s.cfg.brnc = x.brnc;
      }

      runProj(s);
    } else {
      log("[" + n + "] pull failed");
      s.sts = PSts::FAIL;
      s.lErr = "pull failed";
      s.flCnt++;
    }
    return;
  }

  switch (s.sts) {
  case PSts::RUN:
    return;

  case PSts::EXT_OK:
    if (x.hasSet && x.rstrtOnSucc) {
      log("[" + n + "] restart_on_success=true, restarting...");
      runProj(s);
    }
    return;

  case PSts::FAIL: {
    bool sr = !x.hasSet || x.rstrtOnExit;
    int mr = emr(x);
    if (sr && s.flCnt < mr) {
      log("[" + n + "] restarting after failure (" + std::to_string(s.flCnt) +
          "/" + std::to_string(mr) + ")");
      runProj(s);
    }
    return;
  }

  case PSts::STOP:
    return;

  case PSts::IDLE:
    runProj(s);
    return;

  default:
    return;
  }
}

void Scheduler::runProj(PStt &s) {
  std::string xp = s.cfg.lPTH + "/ndx.toml";
  struct stat st;
  if (stat(xp.c_str(), &st) != 0) {
    log("[" + s.cfg.n + "] no ndx.toml found");
    s.sts = PSts::FAIL;
    s.lErr = "ndx.toml not found";
    return;
  }

  NFle x = NFle::prs(xp);
  const NCmds *c = x.mPlt();

  if (!c) {
    log("[" + s.cfg.n + "] no matching platform block in ndx.toml (current: " +
        plt::toS(plt::dtct()) + ")");
    s.sts = PSts::FAIL;
    s.lErr = "no matching platform in ndx.toml";
    return;
  }

  if (c->cmds.empty()) {
    log("[" + s.cfg.n + "] no commands for platform " + c->pltSpec);
    s.sts = PSts::IDLE;
    return;
  }

  std::string lp = s.logP();
  if (!lp.empty()) {
    struct stat lst;
    if (stat(lp.c_str(), &lst) == 0 && lst.st_size > 0) {
      arcLog(lp, s.cfg.n);
    }
    FILE *w = fopen(lp.c_str(), "w");
    if (w)
      fclose(w);
  }

  log("[" + s.cfg.n + "] running commands for platform: " + c->pltSpec);
  s.sts = PSts::RUN;
  s.lRun = std::chrono::steady_clock::now();
  s.strtOnce = true;
  s.lExt = -1;

  pid_t pid = rnr::rCmdsBg(c->cmds, s.cfg.lPTH, s.logP());
  if (pid > 0) {
    s.pid = pid;
    log("[" + s.cfg.n + "] started with pid " + std::to_string(pid));
  } else {
    s.sts = PSts::FAIL;
    s.lErr = "failed to fork process";
    s.flCnt++;
    log("[" + s.cfg.n + "] failed to start");
  }
}

void Scheduler::stpPrjInt(PStt &s) {
  if (s.pid > 0) {
    kill(-s.pid, SIGTERM);
    rnr::kPrc(s.pid);
    s.pid = -1;
  }
  s.sts = PSts::STOP;
}

void Scheduler::arcLog(const std::string &lp, const std::string &pn) {
  struct stat st;
  if (stat(lp.c_str(), &st) != 0 || st.st_size == 0)
    return;

  cmkdir(cfg.logArcvDir);

  std::string ts = tsStr();
  std::string ak = pn + "_" + ts + ".log.gz";
  std::string ap = cfg.logArcvDir + "/" + ak;

  std::string tl = cfg.logArcvDir + "/" + pn + "_" + ts + ".log";
  std::string cp = "cp '" + lp + "' '" + tl + "'";
  system(cp.c_str());

  std::string gz = "gzip '" + tl + "'";
  int rc = system(gz.c_str());

  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    log("archived log: " + ak);
  } else {
    log("archived log (uncompressed): " + pn + "_" + ts + ".log");
  }
}

void Scheduler::clnLog() {
  if (cfg.logArcvDir.empty())
    return;

  DIR *d = opendir(cfg.logArcvDir.c_str());
  if (!d)
    return;

  auto now = std::chrono::system_clock::now();

  std::map<std::string, int> ro;
  for (const auto &[n, s] : stts) {
    NDSet x = rSet(s.cfg.lPTH);
    if (x.logRetDays > 0) {
      ro[n] = x.logRetDays;
    }
  }

  struct dirent *e;
  while ((e = readdir(d)) != nullptr) {
    std::string fn = e->d_name;
    if (fn == "." || fn == "..")
      continue;

    bool isA = false;
    if (fn.size() > 3 && fn.substr(fn.size() - 3) == ".gz")
      isA = true;
    if (fn.size() > 4 && fn.substr(fn.size() - 4) == ".log")
      isA = true;
    if (!isA)
      continue;

    std::string fp = cfg.logArcvDir + "/" + fn;
    struct stat fst;
    if (stat(fp.c_str(), &fst) != 0)
      continue;

    int rd = cfg.logRetDays;
    for (const auto &[pn, dys] : ro) {
      if (fn.find(pn + "_") == 0) {
        rd = dys;
        break;
      }
    }

    auto ft = std::chrono::system_clock::from_time_t(fst.st_mtime);
    auto age =
        std::chrono::duration_cast<std::chrono::seconds>(now - ft).count();

    if (age > rd * 86400) {
      if (unlink(fp.c_str()) == 0) {
        log("deleted old log archive: " + fn);
      }
    }
  }

  closedir(d);
}

void Scheduler::runGc() {
  log("running git gc on all projects...");
  for (auto &[n, s] : stts) {
    struct stat st;
    if (stat((s.cfg.lPTH + "/.git").c_str(), &st) != 0)
      continue;

    std::string out;
    int rc = mgit::rIn(s.cfg.lPTH, "gc --auto --quiet", &out);
    if (rc == 0) {
      log("[" + n + "] git gc done");
    } else {
      log("[" + n + "] git gc failed: " + out);
    }
  }
}

void Scheduler::svCfg() {
  cfg.projs.clear();
  for (const auto &[n, s] : stts) {
    cfg.projs.push_back(s.cfg);
  }
  cfg.save();
}

std::string Scheduler::aProj(const std::string &n, const std::string &url,
                             const std::string &br) {
  {
    std::lock_guard<std::mutex> lk(mtx);

    if (stts.find(n) != stts.end()) {
      return "err: project '" + n + "' already exists\n";
    }

    PEnt e;
    e.n = n;
    e.gURL = url;
    e.brnc = br;
    e.enabled = true;
    e.lPTH = cfg.prjDir + "/" + n;

    PStt s;
    s.cfg = e;
    s.sts = PSts::IDLE;
    stts[n] = s;

    svCfg();
    log("project added: " + n + " -> " + url);
  }
  wake();
  return "ok: project '" + n + "' added\n";
}

std::string Scheduler::rProj(const std::string &n) {
  {
    std::lock_guard<std::mutex> lk(mtx);

    auto it = stts.find(n);
    if (it == stts.end()) {
      return "ert: project '" + n + "' not found\n";
    }

    if (it->second.sts == PSts::RUN) {
      stpPrjInt(it->second);
    }

    stts.erase(it);
    svCfg();
    log("project removed: " + n);
  }
  return "ok: project '" + n + "' removed (files not deleted)\n";
}

std::string Scheduler::eProj(const std::string &n) {
  {
    std::lock_guard<std::mutex> lk(mtx);

    auto it = stts.find(n);
    if (it == stts.end())
      return "err: project '" + n + "' not found\n";

    it->second.cfg.enabled = true;
    it->second.sts = PSts::IDLE;
    it->second.strtOnce = false;
    it->second.flCnt = 0;
    svCfg();
  }
  wake();
  return "ok: project '" + n + "' enabled\n";
}

std::string Scheduler::dProj(const std::string &n) {
  {
    std::lock_guard<std::mutex> lk(mtx);

    auto it = stts.find(n);
    if (it == stts.end())
      return "err: project '" + n + "' not found\n";

    if (it->second.sts == PSts::RUN) {
      stpPrjInt(it->second);
    }
    it->second.cfg.enabled = false;
    it->second.sts = PSts::DIS;
    svCfg();
  }
  return "ok: project '" + n + "' disabled\n";
}

std::string Scheduler::rsProj(const std::string &n) {
  {
    std::lock_guard<std::mutex> lk(mtx);

    auto it = stts.find(n);
    if (it == stts.end())
      return "err: project '" + n + "' not found\n";

    if (it->second.sts == PSts::RUN) {
      stpPrjInt(it->second);
    }
    it->second.sts = PSts::IDLE;
    it->second.strtOnce = false;
    it->second.flCnt = 0;
  }
  wake();
  return "ok: project '" + n + "' restarting\n";
}

std::string Scheduler::sProj(const std::string &n) {
  {
    std::lock_guard<std::mutex> lk(mtx);

    auto it = stts.find(n);
    if (it == stts.end())
      return "err: project '" + n + "' not found\n";

    stpPrjInt(it->second);
  }
  return "ok: project '" + n + "' stopped\n";
}

std::string Scheduler::prjSts(const std::string &n) {
  std::lock_guard<std::mutex> lk(mtx);

  auto it = stts.find(n);
  if (it == stts.end())
    return "err: project '" + n + "' not found\n";

  const auto &s = it->second;
  std::stringstream ss;
  ss << "project: " << s.cfg.n << "\n";
  ss << "  status:     " << s.stsS() << "\n";
  ss << "  git:        " << s.cfg.gURL << "\n";
  ss << "  branch:     " << s.cfg.brnc << "\n";
  ss << "  path:       " << s.cfg.lPTH << "\n";
  ss << "  enabled:    " << (s.cfg.enabled ? "yes" : "no") << "\n";
  ss << "  commit:     " << (s.lCmt.empty() ? "(none)" : s.lCmt) << "\n";
  ss << "  pid:        " << (s.pid > 0 ? std::to_string(s.pid) : "(none)")
     << "\n";
  if (s.lExt >= 0)
    ss << "  exit code:  " << s.lExt << "\n";
  ss << "  failures:   " << s.flCnt << "\n";

  NDSet x = rSet(s.cfg.lPTH);
  if (x.hasSet) {
    ss << "  ndx.toml overrides:\n";
    if (x.pllIntvl > 0)
      ss << "    poll_interval:         " << x.pllIntvl << " min\n";
    if (x.logRetDays > 0)
      ss << "    log_retention:         " << x.logRetDays << " days\n";
    if (x.maxRetries >= 0)
      ss << "    max_retries:           " << x.maxRetries << "\n";
    ss << "    restart_on_exit:       " << (x.rstrtOnExit ? "true" : "false")
       << "\n";
    ss << "    restart_on_success:    " << (x.rstrtOnSucc ? "true" : "false")
       << "\n";
    if (!x.brnc.empty())
      ss << "    branch:                " << x.brnc << "\n";
  }

  if (!s.lErr.empty())
    ss << "  last error: " << s.lErr << "\n";
  return ss.str();
}

std::string Scheduler::lstPrj() {
  std::lock_guard<std::mutex> lk(mtx);

  if (stts.empty())
    return "no projects configured\n";

  std::stringstream ss;
  ss << "projects:\n";
  for (const auto &[n, s] : stts) {
    ss << "  " << n << " [" << s.stsS() << "]";
    if (s.pid > 0)
      ss << " (pid " << s.pid << ")";
    if (!s.lCmt.empty())
      ss << " @" << s.lCmt;
    ss << "\n";
  }
  return ss.str();
}

std::string Scheduler::getLog(const std::string &n, int lns) {
  std::lock_guard<std::mutex> lk(mtx);

  if (n == "daemon" || n == "self") {
    return tFile(cfg.logFile, lns);
  }

  auto it = stts.find(n);
  if (it == stts.end())
    return "err: project '" + n + "' not found\n";

  return tFile(it->second.logP(), lns);
}

std::string Scheduler::sPllInt(int min) {
  std::lock_guard<std::mutex> lk(mtx);
  if (min < 1)
    return "err: interval must be >= 1 minute\n";
  cfg.dPllIntvlMin = min;
  svCfg();
  return "ok: poll interval set to " + std::to_string(min) + " minutes\n";
}

std::string Scheduler::sLogRet(int dys) {
  std::lock_guard<std::mutex> lk(mtx);
  if (dys < 1)
    return "err: retention must be >= 1 day\n";
  cfg.logRetDays = dys;
  svCfg();
  return "ok: log retention set to " + std::to_string(dys) + " days\n";
}

std::string Scheduler::getCfg() {
  std::lock_guard<std::mutex> lk(mtx);
  std::stringstream ss;
  ss << "config:\n";
  ss << "  config file:        " << DConf::confPth() << "\n";
  ss << "  projects dir:       " << cfg.prjDir << "\n";
  ss << "  log file:           " << cfg.logFile << "\n";
  ss << "  log archive dir:    " << cfg.logArcvDir << "\n";
  ss << "  log retention:      " << cfg.logRetDays << " days\n";
  ss << "  poll interval:      " << cfg.dPllIntvlMin << " min\n";
  ss << "  platform:           " << plt::toS(plt::dtct()) << "\n";
  ss << "  projects:           " << stts.size() << "\n";
  return ss.str();
}

std::string Scheduler::reqShtDn() {
  shtDn = true;
  wake();
  return "ok: shutting down\n";
}
