// Created by Unium on 12.02.26

#include "ipc.hpp"
#include "config.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

static std::string trim(const std::string &s) {
  size_t st = s.find_first_not_of(" \t\r\n");
  if (st == std::string::npos)
    return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(st, e - st + 1);
}

static std::vector<std::string> spl(const std::string &s) {
  std::vector<std::string> p;
  std::istringstream iss(s);
  std::string t;
  while (iss >> t) {
    p.push_back(t);
  }
  return p;
}

ISvr::ISvr(Scheduler &s) : sch(s) {}

ISvr::~ISvr() { stp(); }

void ISvr::strt() {
  std::string sp = DConf::sockPth();
  unlink(sp.c_str());

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    std::cerr << "err: socket: " << strerror(errno) << "\n";
    return;
  }

  struct sockaddr_un a;
  memset(&a, 0, sizeof(a));
  a.sun_family = AF_UNIX;
  strncpy(a.sun_path, sp.c_str(), sizeof(a.sun_path) - 1);

  if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
    std::cerr << "err: bind: " << strerror(errno) << "\n";
    close(fd);
    fd = -1;
    return;
  }

  if (listen(fd, 5) < 0) {
    std::cerr << "err: listen: " << strerror(errno) << "\n";
    close(fd);
    fd = -1;
    return;
  }

  run = true;
  th = std::thread(&ISvr::loop, this);
}

void ISvr::stp() {
  run = false;
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
    fd = -1;
  }
  if (th.joinable()) {
    th.join();
  }
  unlink(DConf::sockPth().c_str());
}

void ISvr::loop() {
  while (run) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int r = select(fd + 1, &fds, nullptr, nullptr, &tv);
    if (r <= 0)
      continue;

    int c = accept(fd, nullptr, nullptr);
    if (c < 0)
      continue;

    hCl(c);
  }
}

void ISvr::hCl(int c) {
  char b[4096];
  memset(b, 0, sizeof(b));

  ssize_t n = read(c, b, sizeof(b) - 1);
  if (n <= 0) {
    close(c);
    return;
  }

  std::string cmd(b, n);
  std::string r = dsp(trim(cmd));

  write(c, r.c_str(), r.size());
  close(c);
}

std::string ISvr::dsp(const std::string &c) {
  auto p = spl(c);
  if (p.empty())
    return "err: empty\n";

  std::string k = p[0];

  if (k == "ping")
    return "pong\n";

  if (k == "add") {
    if (p.size() < 3)
      return "usage: add <name> <git_url> [branch]\n";
    std::string b = p.size() >= 4 ? p[3] : "main";
    return sch.aProj(p[1], p[2], b);
  }

  if (k == "remove" || k == "rm") {
    if (p.size() < 2)
      return "usage: remove <name>\n";
    return sch.rProj(p[1]);
  }

  if (k == "enable") {
    if (p.size() < 2)
      return "usage: enable <name>\n";
    return sch.eProj(p[1]);
  }

  if (k == "disable") {
    if (p.size() < 2)
      return "usage: disable <name>\n";
    return sch.dProj(p[1]);
  }

  if (k == "restart") {
    if (p.size() < 2)
      return "usage: restart <name>\n";
    return sch.rsProj(p[1]);
  }

  if (k == "stop") {
    if (p.size() < 2)
      return "usage: stop <name>\n";
    return sch.sProj(p[1]);
  }

  if (k == "status") {
    if (p.size() < 2)
      return sch.lstPrj();
    return sch.prjSts(p[1]);
  }

  if (k == "list" || k == "ls") {
    return sch.lstPrj();
  }

  if (k == "logs" || k == "log") {
    if (p.size() < 2)
      return "usage: logs <name> [lines]\n";
    int l = 50;
    if (p.size() >= 3) {
      try {
        l = std::stoi(p[2]);
      } catch (...) {
      }
    }
    return sch.getLog(p[1], l);
  }

  if (k == "config" || k == "info") {
    return sch.getCfg();
  }

  if (k == "set") {
    if (p.size() < 3)
      return "usage: set <key> <value>\n";
    if (p[1] == "poll_interval") {
      try {
        return sch.sPllInt(std::stoi(p[2]));
      } catch (...) {
        return "err: invalid\n";
      }
    }
    if (p[1] == "log_retention") {
      try {
        return sch.sLogRet(std::stoi(p[2]));
      } catch (...) {
        return "err: invalid\n";
      }
    }
    return "err: unknown setting '" + p[1] + "'\n";
  }

  if (k == "reload") {
    sch.rldCfg();
    return "ok: reloaded\n";
  }

  if (k == "shutdown" || k == "exit" || k == "quit") {
    return sch.reqShtDn();
  }

  if (k == "help") {
    return "commands: add remove enable disable restart stop status list logs "
           "config set reload shutdown ping help\n";
  }

  return "err: unknown '" + k + "'\n";
}
