// Created by Unium on 12.02.26

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

// --- from daemon/config.cpp
static std::string gHome() {
  const char *h = std::getenv("HOME");
  return h ? std::string(h) : "/tmp";
}
static std::string confDir() { return gHome() + "/.config/ndatmx"; }
static std::string confPth() { return confDir() + "/config.toml"; }
static std::string pidPth() { return confDir() + "/ndatmxd.pid"; }
static std::string sockPth() {
  const char *tmp = std::getenv("TMPDIR");
  std::string base = tmp ? std::string(tmp) : "/tmp";
  return base + "/ndatmx.sock";
}

// --- to daemon/ipc.cpp
static std::string sendCmd(const std::string &cmd) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return "err: cannot create socket\n";
  }

  struct sockaddr_un a;
  memset(&a, 0, sizeof(a));
  a.sun_family = AF_UNIX;
  strncpy(a.sun_path, sockPth().c_str(), sizeof(a.sun_path) - 1);

  if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
    close(fd);
    return "err: cannot connect to daemon (make sure is ndatmxd running)\n";
  }

  write(fd, cmd.c_str(), cmd.size());
  shutdown(fd, SHUT_WR);
  std::string response;
  char buf[4096];
  while (true) {
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0)
      break;
    buf[n] = '\0';
    response += buf;
  }

  close(fd);

  auto pos = response.find("__SHUTDOWN__");
  if (pos != std::string::npos) {
    response = response.substr(0, pos);
  }

  return response;
}

static bool dRunning() {
  std::ifstream f(pidPth());
  if (!f.is_open())
    return false;
  pid_t pid;
  f >> pid;
  if (pid <= 0)
    return false;
  return kill(pid, 0) == 0;
}

static void pUsage() {
  std::cout <<
      R"(ndatmx

usage: ndatmx <command> [args...]

commands:
  add <name> <git_url> [branch]    add a project (branch defaults to 'main')
  remove/rm <name>                 remove a project
  enable <name>                    enable a disabled project
  disable <name>                   disable a project (stops it)
  restart <name>                   restart a project
  stop <name>                      stop a running project
  status [name]                    show status (all or specific)
  list/ls                          list all projects
  logs <name|daemon> [lines]       show project or daemon logs
  config                           show current config
  set <key> <value>                change a setting
    poll_interval <minutes>          git poll interval
  reload                           reload config from disk
  shutdown                         stop the daemon
  start-daemon [-f]                start the daemon (-f = foreground)
  ping                             check if daemon is alive
  help                             show this help
)";
}

int main(int argc, char **argv) {
  if (argc < 2) {
    pUsage();
    return 0;
  }
  std::string cmd = argv[1];
  if (cmd == "help" || cmd == "--help" || cmd == "-h") {
    pUsage();
    return 0;
  }

  if (cmd == "start-daemon" || cmd == "start") {
    if (dRunning()) {
      std::cout << "daemon is already running\n";
      return 0;
    }

    std::string daemon_cmd = "ndatmxd";
    for (int i = 2; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "-f" || a == "--foreground") {
        daemon_cmd += " -f";
      }
    }

    int rc = system(daemon_cmd.c_str());
    return WEXITSTATUS(rc);
  }

  std::string ipc_cmd;
  for (int i = 1; i < argc; ++i) {
    if (i > 1)
      ipc_cmd += " ";
    ipc_cmd += argv[i];
  }

  std::string response = sendCmd(ipc_cmd);
  std::cout << response;

  if (response.find("err:") == 0)
    return 1;
  return 0;
}
