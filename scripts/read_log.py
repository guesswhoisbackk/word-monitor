import serial, time, sys

port = sys.argv[1] if len(sys.argv) > 1 else "COM6"
seconds = int(sys.argv[2]) if len(sys.argv) > 2 else 15
noreset = len(sys.argv) > 3

s = serial.Serial(port, 115200, timeout=1)
# release handshake lines so the board is not held in reset
s.dtr = False
s.rts = False
if not noreset:
    # ESP32 reset via DTR/RTS toggle
    s.rts = True
    time.sleep(0.1)
    s.rts = False

end = time.time() + seconds
while time.time() < end:
    line = s.readline()
    if line:
        print(line.decode("utf-8", "replace"), end="")
s.close()
