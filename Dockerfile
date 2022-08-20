FROM ubuntu:22.04

RUN apt-get update

RUN apt-get -y install build-essential ninja-build cmake clang-11 ccache protobuf-compiler libsdl2-dev libenet-dev locales
RUN locale-gen en_US.UTF-8

RUN ln -s /usr/bin/clang++-11 /usr/bin/clang++

COPY . /pleroma

WORKDIR /pleroma

RUN cd /pleroma && bash gen_make.sh

RUN cd /pleroma && make

ENV LC_ALL en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US.UTF-8
