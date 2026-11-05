"""Capture the actual LVGL display over USB; no framebuffer allocation on ESP32.

Usage: python scripts/capture_screen.py COM4 build/screen.png [words|settings|answer]
"""
import struct
import sys
import time
import serial
from PIL import Image

port, output = sys.argv[1:3]
s = serial.Serial()
s.port = port
s.baudrate = 115200
s.timeout = 1
s.dtr = False
s.rts = False
s.open()
time.sleep(2)
s.reset_input_buffer()
if len(sys.argv) > 3:
    s.write(b's' if sys.argv[3] == 'settings' else b'bfr' if sys.argv[3] == 'answer' else b'bf')
    time.sleep(1)
s.write(b'!')
data = bytearray()
deadline = time.monotonic() + 45
while time.monotonic() < deadline:
    data.extend(s.read(max(1, s.in_waiting)))
    if data.endswith(b'WMEND\r\n'):
        break
s.close()
canvas = Image.new('RGB', (240, 320))
pos = covered = 0
while True:
    pos = data.find(b'WMFR', pos)
    if pos < 0:
        break
    x, y, w, h = struct.unpack_from('<4H', data, pos + 4)
    start = pos + 12
    end = start + w * h * 2
    if end > len(data) or x + w > 240 or y + h > 320:
        raise RuntimeError('Incomplete display capture')
    raw = data[start:end]
    rgb = bytearray()
    for i in range(0, len(raw), 2):
        pixel = (raw[i] << 8) | raw[i+1]
        rgb.extend((((pixel >> 11) & 31) * 255 // 31, ((pixel >> 5) & 63) * 255 // 63, (pixel & 31) * 255 // 31))
    canvas.paste(Image.frombytes('RGB', (w,h), bytes(rgb)), (x,y))
    covered += w*h
    pos = end
if covered < 240*320:
    raise RuntimeError(f'Only captured {covered} pixels; wait for boot and retry')
canvas.save(output)
print(f'Saved {output}: {covered} pixels from device')
