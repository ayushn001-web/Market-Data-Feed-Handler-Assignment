#!/bin/bash
# Runs server and client together in one terminal (tmux) or separate terminals

BIN=./build

# Check if tmux is available
if command -v tmux &>/dev/null; then
    tmux new-session -d -s feed_demo -x 220 -y 50
    tmux send-keys -t feed_demo "$BIN/exchange_simulator 9876 100 50000" C-m
    sleep 1
    tmux split-window -h -t feed_demo
    tmux send-keys -t feed_demo "$BIN/feed_handler 127.0.0.1 9876" C-m
    tmux attach-session -t feed_demo
else
    echo "Starting server in background..."
    $BIN/exchange_simulator 9876 100 50000 &
    SERVER_PID=$!
    sleep 1
    echo "Starting client..."
    $BIN/feed_handler 127.0.0.1 9876
    kill $SERVER_PID 2>/dev/null
fi