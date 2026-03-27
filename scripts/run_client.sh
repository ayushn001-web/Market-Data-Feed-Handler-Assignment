#!/bin/bash
HOST=${1:-127.0.0.1}
PORT=${2:-9876}
echo "Starting Feed Handler: $HOST:$PORT"
./build/feed_handler $HOST $PORT