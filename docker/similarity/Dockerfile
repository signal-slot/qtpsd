FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    curl \
    jq \
    python3 \
    python3-pil \
    fonts-dejavu-core \
    git \
    xz-utils \
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

ARG FLUTTER_VERSION=""
RUN set -eux; \
    if [ -z "${FLUTTER_VERSION}" ]; then \
      curl -fsSL "https://storage.googleapis.com/flutter_infra_release/releases/releases_linux.json" -o /tmp/flutter_releases.json; \
      FLUTTER_VERSION="$(jq -r '.current_release.stable as $h | .releases[] | select(.hash == $h) | .version' /tmp/flutter_releases.json)"; \
    fi; \
    curl -fsSL "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz" -o /tmp/flutter.tar.xz \
    && mkdir -p /opt \
    && tar -xJf /tmp/flutter.tar.xz -C /opt \
    && rm -f /tmp/flutter.tar.xz /tmp/flutter_releases.json

RUN curl https://sh.rustup.rs -sSf | bash -s -- -y
ENV PATH="/opt/flutter/bin:/root/.cargo/bin:${PATH}"
RUN git config --global --add safe.directory /opt/flutter \
    && flutter config --no-analytics \
    && flutter precache
RUN cargo install slint-viewer --locked

WORKDIR /workspace

ENTRYPOINT ["/bin/bash"]
