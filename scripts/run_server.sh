#!/bin/bash
PORT=${1:-9876}
SYMBOLS=${2:-100}
TICK_RATE=${3:-10000}
echo "Starting Exchange Simulator: port=$PORT symbols=$SYMBOLS tick_rate=$TICK_RATE"
./build/exchange_simulator $PORT $SYMBOLS $TICK_RATE