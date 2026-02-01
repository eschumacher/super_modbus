FROM ubuntu:jammy
RUN apt update -y
RUN apt upgrade -y
# Add LLVM repository for clang-format-18
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - \
    && echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-18 main" > /etc/apt/sources.list.d/llvm.list \
    && apt update -y
RUN apt install -y sudo vim cmake cppcheck lcov curl llvm clang clangd clang-tidy clang-format-18 wget git build-essential bash-completion \
    && ln -sf /usr/bin/clang-format-18 /usr/bin/clang-format || true
ARG USERNAME=coder
ARG USER_UID=1001
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME
USER coder
WORKDIR /home/coder
RUN /bin/bash
