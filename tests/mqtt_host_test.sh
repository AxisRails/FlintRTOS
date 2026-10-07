#!/bin/sh
# FlintRTOS - end-to-end test of port/mqtt/flint_mqtt.c against a real broker.
# Usage: tests/mqtt_host_test.sh <binary> [posix|lwip] [port]
#   posix: tests/mqtt_host_test.c  (app over POSIX sockets)
#   lwip : tests/mqtt_lwip_test.c  (app + transport_raw.c + lwIP over a TAP; needs root)
set -u
BIN=$1
MODE=${2:-posix}
PORT=${3:-18830}
ID=flint-$MODE-test
DIR=$(mktemp -d)
fail=0

printf 'listener %s 0.0.0.0\nallow_anonymous true\n' "$PORT" > "$DIR/mosq.conf"
mosquitto -c "$DIR/mosq.conf" > "$DIR/broker.log" 2>&1 &
BROKER=$!
sleep 0.5

mosquitto_sub -p "$PORT" -v -t "flint/$ID/#" > "$DIR/sub.log" 2>&1 &
SUB=$!
sleep 0.3

if [ "$MODE" = lwip ]; then
    "$BIN" 6 "$PORT" "$ID" > "$DIR/dev.log" 2>&1 &
else
    "$BIN" 127.0.0.1 "$PORT" "$ID" 6 > "$DIR/dev.log" 2>&1 &
fi
DEV=$!
sleep 2
for c in ping uptime "led on" "led toggle" "led blink" ptp stats help bogus; do
    mosquitto_pub -p "$PORT" -t "flint/$ID/cmd" -m "$c"
    sleep 0.15
done
wait $DEV
sleep 0.5
kill $SUB $BROKER 2>/dev/null
wait 2>/dev/null

check() {   # check <description> <grep pattern> <file>
    if grep -q -- "$2" "$3"; then echo "  ok   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "--- device log"; cat "$DIR/dev.log"
echo "--- broker traffic"; cat "$DIR/sub.log"
echo "--- checks"
check "retained online status"   "flint/$ID/status {\"state\":\"online\""   "$DIR/sub.log"
check "PTP telemetry published"  "flint/$ID/ptp {\"state\":\"LOCKED\""      "$DIR/sub.log"
check "stats published"          "flint/$ID/stats {\"uptime_s\""           "$DIR/sub.log"
check "ping -> pong"             "\"cmd\":\"ping\",\"reply\":\"pong\""     "$DIR/sub.log"
check "uptime reply"             "\"cmd\":\"uptime\",\"uptime_s\""          "$DIR/sub.log"
check "led on"                   "\"cmd\":\"led on\",\"led\":\"on\""        "$DIR/sub.log"
check "led toggle -> off"        "\"cmd\":\"led toggle\",\"led\":\"off\""   "$DIR/sub.log"
check "led bad arg rejected"     "\"cmd\":\"led blink\",\"error\""          "$DIR/sub.log"
check "help"                     "\"commands\":"                            "$DIR/sub.log"
check "unknown command"          "\"cmd\":\"bogus\",\"error\""              "$DIR/sub.log"
check "Last Will -> offline"     "flint/$ID/status {\"state\":\"offline\"}" "$DIR/sub.log"
check "9 commands handled"       "commands=9"                               "$DIR/dev.log"
rm -rf "$DIR"
exit $fail
