FROM alpine:edge AS alpine

RUN apk add --no-cache \
    bash \
    curl \
    gcc \
    git \
    linux-headers \
    gcompat \
    make \
    meson \
    ncurses \
    perl \
    wget
    # musl-dev

# ----------------------------------------------------------------------------

FROM ubuntu:24.04 AS ubuntu

ENV PATH=/root/.local/bin:$PATH
ENV CC=gcc-14

RUN apt-get update && apt-get install -y \
    build-essential \
    curl \
    gcc-14 \
    make \
    musl-tools \
    ninja-build \
    pipx \
    wget

RUN pipx install meson
