FROM ubuntu:26.04

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
    qml6-module-qtquick-effects \
    libgl1-mesa-dev \
    libgl1-mesa-dri \
    libegl1-mesa-dev \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    libx11-6 \
    libxcursor1 \
    libxrandr2 \
    libxi6 \
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

# Build LVGL headless capture harness. The exporter-generated main_screen.c
# is compiled against this per export by scripts/lvgl2png.sh, so the build
# tree (LVGL sources + static libs) must stay in the image.
COPY tools/lvgl_capture /opt/lvgl_capture_src
RUN cmake -S /opt/lvgl_capture_src -B /opt/lvgl_capture_build -G Ninja \
    && cmake --build /opt/lvgl_capture_build \
    && chmod -R a+rX /opt/lvgl_capture_src /opt/lvgl_capture_build
ENV LVGL_CAPTURE_ENV=/opt/lvgl_capture_build/capture_env.sh

WORKDIR /workspace

ENTRYPOINT ["/bin/bash"]
