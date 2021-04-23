FROM ubuntu:latest
RUN apt-get update
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install tzdata
RUN apt-get install -y cmake make wget lsb-release software-properties-common ca-certificates git zlib1g-dev
RUN bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
RUN ln /usr/bin/llc-12 /usr/bin/llc
RUN ln /usr/bin/clang++-12 /usr/bin/clang++