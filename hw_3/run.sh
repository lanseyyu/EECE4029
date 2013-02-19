#!/bin/zsh

sudo ifconfig os0 10.0.0.1 netmask 255.255.255.0 broadcast 10.0.0.255
sudo ifconfig os1 10.0.1.2 netmask 255.255.255.0 broadcast 10.0.1.255
