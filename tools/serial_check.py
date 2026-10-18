import argparse
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='COM8')
parser.add_argument('--seconds', type=float, default=12)
parser.add_argument('--send', default='')
args = parser.parse_args()
with serial.Serial(args.port, 115200, timeout=0.2) as connection:
    if args.send:
        connection.write(args.send.encode())
    end = time.monotonic() + args.seconds
    while time.monotonic() < end:
        data = connection.read(4096)
        if data:
            print(data.decode(errors='replace'), end='', flush=True)
