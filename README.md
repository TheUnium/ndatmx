# ndatmx

`ndatmx` is a daemon based project manager thingy i made to automate the
fetching and running projects. it monitors git repos for changes, pulls
commits, and then runs the project based on the conf file the project has.

stuff it has:

- it can pull shit from git repos
- it can start/stop/restart projects
- technically it can act as a dependency manager for projects?
- a lot of logging, and log rotation for projects
- a cli

![showcase image](./assets/showcase.png)

## how to build

to build the project, you need gcc/clang and cmake

```bash
mkdir build
cd build
cmake ..
make
```

output:

- `ndatmxd`: the daemon
- `ndatmx`: the cli tool

## usage

### 0. example config

```toml
[daemon]
poll_interval = 5
log_retention = 14
restart_on_exit = true
restart_on_success = false
max_retries = 5

[commands:termux]
1="npm install"
2="npm run start"

[commands:debian13]
1="bun install"
2="bun run src/index.ts"
```

### 1. start the daemon

start the daemon using the cli:

```bash
ndatmx start-daemon
```

if you need to run it in the foreground to debug, then:

```bash
ndatmx start-daemon -f # or ndatmxd -f
```

### 2. add and manage projects

once the daemon is running you can use the cli to add/manage projects

#### adding a project

```bash
ndatmx add <name> <git url> [git branch]
```

e.g.:

```bash
ndatmx add test-proj https://github.com/TheUnium/ndatmx-test.git main
```

#### list projects

```bash
ndatmx list
```

#### check status

```bash
ndatmx status [project name]
```

#### view logs

```bash
ndatmx logs <project name> [num of lines from the end]
```

#### stop/start/restart

```bash
ndatmx stop <project name>
ndatmx start <project name>
ndatmx restart <project name>
```

#### remove a project

```bash
ndatmx remove <project name>
```

#### config

you can view and modify the config:

```bash
ndatmx config
ndatmx set poll_interval 5
```
