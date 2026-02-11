FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    curl \
    git \
    cmake \
    ninja-build \
    build-essential \
    pkg-config \
    qt6-base-dev \
    qt6-base-private-dev \
    qt6-declarative-dev \
    qt6-declarative-private-dev \
    qt6-tools-dev-tools \
    qml-qt6 \
    qml6-module-qtqml \
    qml6-module-qtqml-models \
    qml6-module-qtqml-workerscript \
    qml6-module-qtquick \
    qml6-module-qtquick-window \
    qml6-module-qtquick-shapes \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    xvfb \
    xdotool \
    imagemagick \
    xauth \
    && rm -rf /var/lib/apt/lists/*

RUN curl https://sh.rustup.rs -sSf | bash -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN cargo install slint-viewer --locked

WORKDIR /workspace

ENTRYPOINT ["/bin/bash"]
