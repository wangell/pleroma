FROM ubuntu:22.04

RUN apt-get -y update && apt-get -y install build-essential ninja-build cmake clang-11 ccache protobuf-compiler libsdl2-dev libsdl2-ttf-dev libenet-dev locales git libssl-dev
RUN locale-gen en_US.UTF-8

RUN ln -s /usr/bin/clang++-11 /usr/bin/clang++

RUN git clone https://github.com/rui314/mold.git && cd mold && git checkout v1.4.2 && make CXX=clang++ && make install

COPY . /pleroma

WORKDIR /pleroma

RUN cd /pleroma && make cmake && make

ENV LC_ALL en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US.UTF-8
