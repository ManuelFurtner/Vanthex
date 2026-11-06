# VANTHEX Linux dedicated server

Build inputs: the complete local VANTHEX project, Unreal Engine **5.8.3**, Linux
amd64 Docker engine and access to Epic's official development image. The image is
pinned to its verified amd64 digest. No engine source or credentials belong in Git.

The public repository omits licensed Fab content. A GitHub clone alone is not the
complete local asset set. Restore those assets through the licensed workflow before
building. The Docker context includes local Content, but excludes .git, Saved,
Intermediate, downloaded source bundles and local credentials. Do not publish the
builder stage or its cache, which contain raw assets and Unreal development tools.

## Build locally

Run from PowerShell 7.2 or newer:

```powershell
./Build/LinuxServer/Build-LinuxServer.ps1
```

The default runtime image is `vanthex-server:local`. The build compiles the
`VanthexServer` target, cooks GRID V4 and Crossroads CTF, and packages a Development server. Logs go
to `Saved/LinuxServer/build-*.log`. Before building, the script pulls the pinned
engine image if necessary and retains it as `vanthex-unreal-build:5.8.3` locally.
This avoids depending only on BuildKit's smaller disposable cache after a failed
compile. Initial download logs are `Saved/LinuxServer/engine-pull-*.log`.
To export a standalone Linux package instead:

```powershell
./Build/LinuxServer/Build-LinuxServer.ps1 -ExportPackage
```

The exported package is in `Saved/LinuxServer/Package`. Keep the whole directory.
Execute `./StartServer.sh` on Linux; the Windows `.exe` cannot substitute for this
Linux executable.

## Run and connect

```powershell
docker run --rm --name vanthex-server -p 7777:7777/udp vanthex-server:local
```

Connect from the matching Windows client to `127.0.0.1:7777` on the same machine,
or to the server's reachable address and published port from another PC. Remote
access requires the appropriate UDP routing/firewall configuration. The server
waits for the first player before creating the roster and starting the match.

For a different port, set `VANTHEX_PORT` and publish that container UDP port. On
Edgegap, use a UDP port mapping named `gameport`; the entrypoint honors
`ARBITRIUM_PORT_GAMEPORT_INTERNAL`. Otherwise the default is 7777. The container
runs as an unprivileged user. Its main process is the server ELF, so SIGTERM and
the exit status are preserved. No credentials are printed by the launcher.

Network version, session and packet checks remain at their Unreal defaults.
Do not bypass compatibility failures: build client and server from matching code
and engine versions. This setup does not create matchmaking, player authentication,
a public server browser, Edgegap deployments or registry credentials.

## Validation

After building the image, run `./Build/LinuxServer/Test-LinuxServer.ps1`. It uses a
unique test container and a loopback-only UDP port, launches two packaged Windows
clients, and checks both joins and an active round without bypassing network
compatibility. It saves logs under `Saved/LinuxServer/vanthex-smoke-*` and removes
only its own test container/processes. This is a startup/connection smoke test;
it does not verify every combat or objective mechanic.

Use `./Build/LinuxServer/Test-LinuxServer.ps1 -RoundCycle` to additionally check
automatic round transitions after elimination, timeout and extraction, and the
automatic next match after seven wins. The server shows the match result for
10 seconds, then resets scores, economy, health and the Core in place while the
two existing Windows clients stay connected. Each new round retains the normal
12-second buy phase. This uses the server-only `-RoundCycleTest` fixture and does
not require a rebuilt client or disabled network compatibility checks.

`python Build/LinuxServer/test_start_server.py` exercises the actual launcher:
default arguments, custom/local and Edgegap ports, invalid ports, missing binaries,
argument boundaries and server exit status. On Windows set `VANTHEX_TEST_BASH` to
Git Bash's executable. These checks do not establish that Unreal compiled or that
a client connected. Build logs and actual runtime tests must establish those facts.

## References

- [Epic's container quick start](https://dev.epicgames.com/documentation/unreal-engine/quick-start-guide-for-using-container-images-in-unreal-engine)
- [Edgegap's Unreal build workflows](https://docs.edgegap.com/unreal-engine/more-build-methods)
- [Docker build-cache garbage collection](https://docs.docker.com/build/cache/garbage-collection/)

Only the cooked runtime image is intended for deployment. Registry publication
and an actual Edgegap deployment are separate steps, requiring the destination
and account access. No paid service is enabled by these scripts.
