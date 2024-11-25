ARG BASE=alpine

# ----------------------------------------------------------------------------

# alpine edge has recent meson version
FROM alpine:edge AS alpine

RUN apk add --no-cache \
    bash \
    curl \
    gcc \
    git \
    linux-headers \
    make \
    meson \
    musl-dev \
    ncurses \
    perl \
    wget

# add musl-gcc so its consistent CC with ubuntu
RUN ln -s $(which gcc) /usr/bin/musl-gcc

# ----------------------------------------------------------------------------

FROM ubuntu:24.04 AS ubuntu

ENV PATH=/root/.local/bin:$PATH

RUN apt-get update && apt-get install -y \
    build-essential \
    curl \
    gcc-14 \
    make \
    musl-tools \
    ninja-build \
    pipx \
    wget

# get latest meson via pipx as apt has ancient version
RUN pipx install meson

# ----------------------------------------------------------------------------

FROM $BASE

ENV CC=musl-gcc
