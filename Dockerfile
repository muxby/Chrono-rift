# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║  CHRONO RIFT — SFML Visualizer Docker Image                             ║
# ║  Build: docker build -t chrono-rift .                                   ║
# ║  Run:   docker run -it --rm -e DISPLAY=:0 \                             ║
# ║              -v /tmp/.X11-unix:/tmp/.X11-unix chrono-rift               ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    cmake \
    libsfml-dev \
    libncurses5-dev \
    libncursesw5-dev \
    libpthread-stubs0-dev \
    libx11-dev \
    fonts-dejavu \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source files
COPY . /app/

# Build the project (clean first to ensure fresh build)
RUN make clean && make all

# Set the default command to run the standalone visualizer
# This launcher handles forking the Arbiter, HIP, and ASP processes.
CMD ["./sfml_ui/chrono_rift_standalone"]
