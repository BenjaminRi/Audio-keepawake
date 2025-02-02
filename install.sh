#!/bin/sh

set -x
set -e

# Install application
#--------------------

# Install binary
mkdir -p $HOME/.local/bin
cp -f ./build/audio-keepawake $HOME/.local/bin

# Install service
#----------------

mkdir -p $HOME/.local/share/systemd/user
cp service/audio-keepawake.service $HOME/.local/share/systemd/user/
systemctl --user daemon-reload
systemctl --user enable audio-keepawake.service
# Created symlink /home/foo/.config/systemd/user/default.target.wants/audio-keepawake.service → /home/foo/.local/share/systemd/user/audio-keepawake.service
systemctl --user start audio-keepawake.service

# Debugging only
#---------------
#systemctl --user status audio-keepawake.service
#systemctl --user stop audio-keepawake.service
#systemctl --user start audio-keepawake.service
#journalctl --user -u audio-keepawake.service -b
