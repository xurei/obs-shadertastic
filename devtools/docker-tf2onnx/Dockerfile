FROM ubuntu:22.04

RUN apt update && apt install -y python3-pip && apt clean
RUN pip install -U pip
RUN pip install -U tensorflow
RUN pip install -U tf2onnx
WORKDIR /workdir
