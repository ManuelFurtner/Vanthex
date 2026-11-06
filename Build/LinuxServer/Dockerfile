# syntax=docker/dockerfile:1
ARG UE_IMAGE=ghcr.io/epicgames/unreal-engine:dev-slim-5.8.3@sha256:07ae3f48e858cda09c906aa6fe7a9665213bd2d7572c823877f71a458a04bd29
FROM ${UE_IMAGE} AS builder
SHELL ["/bin/bash", "-c"]
USER ue4
WORKDIR /tmp/project
COPY --chown=ue4:ue4 . .

# Reject an accidental engine upgrade: the current Windows client uses 5.8.3.
RUN python3 -c 'import json; v=json.load(open("/home/ue4/UnrealEngine/Engine/Build/Build.version")); assert (v["MajorVersion"],v["MinorVersion"],v["PatchVersion"]) == (5,8,3), v'

# The official Linux development image contains a server-enabled installed engine.
# File-based cooking avoids requiring a reachable external Zen store during staging.
RUN --mount=type=cache,id=vanthex-linux-intermediate,target=/tmp/project/Intermediate,uid=1000,gid=1000 \
    --mount=type=cache,id=vanthex-linux-binaries,target=/tmp/project/Binaries,uid=1000,gid=1000 \
    --mount=type=cache,id=vanthex-linux-saved,target=/tmp/project/Saved,uid=1000,gid=1000 \
    --mount=type=cache,id=vanthex-linux-ddc,target=/home/ue4/ddc,uid=1000,gid=1000 \
    env UE-LocalDataCachePath=/home/ue4/ddc \
    /home/ue4/UnrealEngine/Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
    -project=/tmp/project/Vanthex.uproject -nop4 -utf8output -unattended \
    -build -cook -stage -pak -iostore -archive -nodebuginfo \
    -server -noclient -platform=Linux -serverplatform=Linux -serverconfig=Development \
    -map=/Game/Vanthex/Maps/GridV4 -archivedirectory=/tmp/Packaged \
    -ddc=InstalledNoZenLocalFallback \
    '-AdditionalCookerOptions=-SkipZenStore -DDC=InstalledNoZenLocalFallback -LocalDataCachePath=/home/ue4/ddc'

# Optional export target: docker build --target package --output type=local,dest=...
FROM scratch AS package
COPY --from=builder /tmp/Packaged/LinuxServer/ /
COPY --chmod=755 Build/LinuxServer/StartServer.sh /StartServer.sh

FROM ubuntu:22.04 AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 vanthex
WORKDIR /opt/vanthex
COPY --from=builder --chown=10001:10001 /tmp/Packaged/LinuxServer/ ./
COPY --chown=10001:10001 --chmod=755 Build/LinuxServer/StartServer.sh ./StartServer.sh
RUN sed -i 's/\r$//' StartServer.sh && chmod +x Vanthex/Binaries/Linux/VanthexServer
USER 10001:10001
EXPOSE 7777/udp
STOPSIGNAL SIGTERM
ENTRYPOINT ["/opt/vanthex/StartServer.sh"]
