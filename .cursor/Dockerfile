FROM ubuntu:24.04

# Cloud agents are Ubuntu. ka-hgis Release builds stay on the Windows machine
# that has OSGeo4W qgis-dev and Visual Studio 2022. Do not install QGIS here.
RUN apt-get update \
  && apt-get install -y --no-install-recommends ca-certificates git python3 nodejs npm \
  && rm -rf /var/lib/apt/lists/*
