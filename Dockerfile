# Chrono Rift - Docker Build
# Optimized multi-stage build for Ubuntu 22.04

# --- STAGE 1: Build ---
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    cmake \
    libsfml-dev \
    libpthread-stubs0-dev \
    libx11-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source files
COPY . .

# Build all targets
RUN make clean && make all

# --- STAGE 2: Runtime ---
FROM ubuntu:22.04

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libsfml-graphics2.5 \
    libsfml-window2.5 \
    libsfml-system2.5 \
    libx11-6 \
    libgl1-mesa-dri \
    libgl1-mesa-glx \
    libegl1-mesa \
    libxrender1 \
    fonts-dejavu \
    mesa-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled binaries and necessary assets from builder
COPY --from=builder /build/hip/hip ./hip/
COPY --from=builder /build/asp/asp ./asp/
COPY --from=builder /build/sfml_ui/sfml_arbiter ./sfml_ui/
COPY --from=builder /build/sfml_ui/chrono_rift_visualizer ./sfml_ui/
COPY --from=builder /build/sfml_ui/chrono_rift_standalone ./sfml_ui/
COPY --from=builder /build/background ./background/
COPY --from=builder /build/SPRITESHEET ./SPRITESHEET/

# Set environment for X11 (default to :0, can be overridden)
ENV DISPLAY=:0

# The standalone launcher handles forking all processes and managing stdin.
CMD ["./sfml_ui/chrono_rift_standalone"]
