FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update
RUN apt -y upgrade
RUN apt -y install python3
RUN apt -y install python-is-python3
RUN apt -y install libpython2-dev
RUN apt -y install libusb-1.0-0-dev


