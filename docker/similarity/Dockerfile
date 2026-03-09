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
    fonts-roboto \
    fonts-noto-core \
    fonts-noto-color-emoji \
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
    qml6-module-qtquick-layouts \
    qml6-module-qtquick-shapes \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    xvfb \
    xcompmgr \
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

ENV CARGO_HOME=/usr/local/cargo
RUN curl https://sh.rustup.rs -sSf | bash -s -- -y --default-toolchain stable --no-modify-path
ENV PATH="/opt/flutter/bin:${CARGO_HOME}/bin:${PATH}"
ENV PUB_CACHE=/pub-cache
RUN mkdir -p ${PUB_CACHE} \
    && git config --global --add safe.directory /opt/flutter \
    && flutter config --no-analytics \
    && flutter precache \
    && chmod -R a+rX /opt/flutter \
    && chmod -R a+rwX /opt/flutter/.git /opt/flutter/bin/cache \
    && chmod -R a+rwX ${PUB_CACHE}
RUN cargo install slint-viewer --locked

# Build LVGL headless capture tool
COPY tools/lvgl_capture /tmp/lvgl_capture_src
RUN cmake -S /tmp/lvgl_capture_src -B /tmp/lvgl_capture_build -G Ninja \
    && cmake --build /tmp/lvgl_capture_build \
    && cp /tmp/lvgl_capture_build/lvgl_capture /usr/local/bin/lvgl_capture \
    && rm -rf /tmp/lvgl_capture_build /tmp/lvgl_capture_src

WORKDIR /workspace

ENTRYPOINT ["/bin/bash"]
