# README:
# For dev, build and run
#     podman run -td --mount type=bind,src=$PWD,target=/source <dev-image>
# and use the dotnet commands on the now keeping-on-running container like so:
#     podman exec <container> dotnet build
# For executable, build and run
#     podman run -td --mount type=bind,src=$PWD,target=/source <linux-x64-image>
# Get the container name and run
#     podman exec <container> dotnet build --runtime linux-x64 --self-contained --output /source/out && cp -r ./morte/Content ./out && ./out/morte

FROM mcr.microsoft.com/dotnet/sdk:6.0-alpine AS dev
WORKDIR /source/morte

FROM dev AS linux-x64
COPY . /source
WORKDIR /source/morte
RUN dotnet build --runtime linux-x64 --self-contained --output /source/out
