#!/bin/sh

case "$1" in
    *-rc*) echo tag=rc;;
    *-beta*) echo tag=beta;;
    *-alpha*) echo tag=alpha;;
    *) echo tag=latest;;
esac

