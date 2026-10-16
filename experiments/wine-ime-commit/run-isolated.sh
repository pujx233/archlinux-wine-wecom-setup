#!/usr/bin/env bash
# Run under xvfb-run. Intentionally fixed to the disposable experiment tree.
set -euo pipefail
work=/tmp/wecom-complete-fix
export WINEPREFIX="$work/prefix"
export WINEDLLPATH=/usr/lib/wine
export WINESERVER=/usr/bin/wineserver
export WINEDLLOVERRIDES=winemenubuilder.exe=d
export WINEDEBUG="${WECOM_EXPERIMENT_DEBUG:--all}"
unset LD_PRELOAD WINELOADER WINEARCH
wine="$work/isolated/lib/wine/x86_64-unix/wine"
probe=/tmp/wine-ime-commit-probe.exe
[[ ! -L "$WINEPREFIX" && -x "$wine" && -f "$probe" && -f /tmp/wine-ime-commit-probe32.exe ]]
cleanup() {
    "$WINESERVER" -k || true
}
trap cleanup EXIT
if [[ ! -f "$WINEPREFIX/drive_c/windows/system32/kernel32.dll" ]]; then
    /usr/bin/wine wineboot -u > "$work/bootstrap.log" 2>&1
    "$WINESERVER" -w
fi
"$wine" wineboot -u > "$work/wineboot.log" 2>&1
for scenario in 64 32 destroy; do
    sender_probe="$probe"
    receiver_mode=--receiver
    sender_mode=--sender
    if [[ "$scenario" != 64 ]]; then sender_probe=/tmp/wine-ime-commit-probe32.exe; fi
    if [[ "$scenario" == destroy ]]; then
        receiver_mode=--destroy-receiver
        sender_mode=--destroy-sender
    fi
    receiver_log="$work/receiver-$scenario.log"
    sender_log="$work/sender-$scenario.log"
    : > "$receiver_log"
    "$wine" "$probe" "$receiver_mode" > "$receiver_log" 2>&1 &
    receiver=$!
    # Save evidence that the actual process maps the experimental Unix library.
    for ((i=0; i<100; i++)); do
        if grep -q 'READY:' "$receiver_log"; then break; fi
        if ! kill -0 "$receiver" 2>/dev/null; then cat "$receiver_log"; exit 1; fi
        sleep 0.1
    done
    grep '/win32u.so' "/proc/$receiver/maps" > "$work/receiver-$scenario-maps.txt"
    grep -q "$work/isolated/lib/wine/x86_64-unix/win32u.so" "$work/receiver-$scenario-maps.txt"
    "$wine" "$sender_probe" "$sender_mode" > "$sender_log" 2>&1 || {
        cat "$sender_log" "$receiver_log"
        exit 1
    }
    wait "$receiver"
    echo "Scenario: $scenario"
    cat "$sender_log" "$receiver_log"
done
