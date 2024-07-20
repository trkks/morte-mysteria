# README:
# Run the following
#     podman run -td --mount type=bind,src=$PWD,target=/source <image>
# and use the dotnet commands on the now keeping-on-running container like so:
#     podman exec <container> dotnet build

FROM mcr.microsoft.com/dotnet/sdk:6.0-alpine
WORKDIR /source/morte
