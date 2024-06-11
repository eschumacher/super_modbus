FROM ubuntu:jammy
RUN apt update -y
RUN apt upgrade -y
RUN apt install -y sudo cmake cppcheck lcov curl llvm clang clangd clang-tidy clang-format wget git build-essential bash-completion
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