FROM mcr.microsoft.com/dotnet/sdk:6.0-alpine
COPY ./morte/morte.csproj /source/morte/morte.csproj
WORKDIR /source/morte
RUN dotnet add package Newtonsoft.Json --version 11.0.2
RUN dotnet add package MonoGame.Framework.DesktopGL --version 3.8.1.303
WORKDIR /source
COPY . /source
ENTRYPOINT ["dotnet", "build", "--runtime", "linux-x64", "--self-contained", "--output", "/source/out"]
