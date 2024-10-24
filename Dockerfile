FROM alpine:edge

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
