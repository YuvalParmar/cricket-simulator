#!/usr/bin/env python3
"""
Phone Bat Latency Measurement Tool

Run this on the PC to measure end-to-end latency by sending test packets
and measuring round-trip time. Also works as a packet sniffer to analyze
real traffic.
"""

import socket
import struct
import time
import json
import threading
import statistics
from collections import deque
from datetime import datetime
import sys

# Configuration
LISTEN_PORT = 7777
TEST_PORT = 7778
SERVER_IP = "0.0.0.0"  # Listen on all interfaces
PACKET_COUNT = 100
SEND_INTERVAL_MS = 10  # 100Hz

class LatencyTester:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((SERVER_IP, LISTEN_PORT))
        self.sock.settimeout(1.0)

        self.results = []
        self.running = False
        self.sequence = 0
        self.pending = {}  # seq -> send_time

    def send_test_packet(self, target_ip, target_port):
        """Send a test packet with timestamp"""
        self.sequence += 1
        seq = self.sequence
        send_time = time.perf_counter_ns()

        packet = {
            "type": "latency_test",
            "seq": seq,
            "send_time_ns": send_time,
            "send_time_iso": datetime.utcnow().isoformat()
        }

        data = json.dumps(packet).encode('utf-8')
        self.sock.sendto(data, (target_ip, target_port))
        self.pending[seq] = send_time

    def receive_loop(self):
        """Receive and process packets"""
        while self.running:
            try:
                data, addr = self.sock.recvfrom(65536)
                recv_time = time.perf_counter_ns()

                try:
                    packet = json.loads(data.decode('utf-8'))

                    if packet.get("type") == "latency_test":
                        seq = packet.get("seq")
                        send_time = packet.get("send_time_ns")

                        if seq in self.pending:
                            # This is our test packet echoed back
                            rtt_ns = recv_time - self.pending[seq]
                            rtt_ms = rtt_ns / 1_000_000

                            self.results.append({
                                "seq": seq,
                                "rtt_ms": rtt_ms,
                                "recv_time": recv_time,
                                "addr": addr
                            })
                            del self.pending[seq]

                            print(f"Seq {seq}: RTT = {rtt_ms:.2f} ms")

                    elif packet.get("type") == "sensor_frame":
                        # Real sensor frame - measure one-way if clock synced
                        pass

                except json.JSONDecodeError:
                    pass  # Ignore non-JSON packets

            except socket.timeout:
                continue
            except Exception as e:
                print(f"Receive error: {e}")

    def run_test(self, target_ip, target_port, count=PACKET_COUNT):
        """Run latency test"""
        print(f"Starting latency test to {target_ip}:{target_port}")
        print(f"Sending {count} packets at {1000/SEND_INTERVAL_MS:.0f}Hz")
        print("-" * 50)

        self.running = True
        recv_thread = threading.Thread(target=self.receive_loop)
        recv_thread.start()

        try:
            for i in range(count):
                self.send_test_packet(target_ip, target_port)
                time.sleep(SEND_INTERVAL_MS / 1000.0)

            # Wait for responses
            time.sleep(2.0)

        finally:
            self.running = False
            recv_thread.join(timeout=1.0)

        self.print_results()

    def print_results(self):
        """Print latency statistics"""
        if not self.results:
            print("No results received!")
            return

        rtts = [r["rtt_ms"] for r in self.results]

        print("\n" + "=" * 50)
        print("LATENCY TEST RESULTS")
        print("=" * 50)
        print(f"Packets sent:     {PACKET_COUNT}")
        print(f"Packets received: {len(self.results)}")
        print(f"Packet loss:      {(1 - len(self.results)/PACKET_COUNT)*100:.1f}%")
        print()
        print(f"Min RTT:          {min(rtts):.2f} ms")
        print(f"Max RTT:          {max(rtts):.2f} ms")
        print(f"Mean RTT:         {statistics.mean(rtts):.2f} ms")
        print(f"Median RTT:       {statistics.median(rtts):.2f} ms")
        print(f"Stdev RTT:        {statistics.stdev(rtts) if len(rtts) > 1 else 0:.2f} ms")

        # One-way estimate (half RTT)
        print(f"\nEstimated one-way: {statistics.mean(rtts)/2:.2f} ms")
        print(f"Jitter (stdev):   {statistics.stdev(rtts) if len(rtts) > 1 else 0:.2f} ms")

        # Percentiles
        sorted_rtts = sorted(rtts)
        for p in [50, 90, 95, 99]:
            idx = int(len(sorted_rtts) * p / 100)
            print(f"P{p} RTT:           {sorted_rtts[idx]:.2f} ms")

        # Save to file
        filename = f"latency_test_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        with open(filename, 'w') as f:
            json.dump({
                "timestamp": datetime.now().isoformat(),
                "config": {
                    "target_ip": "auto",
                    "packet_count": PACKET_COUNT,
                    "send_interval_ms": SEND_INTERVAL_MS
                },
                "results": self.results,
                "summary": {
                    "min_ms": min(rtts),
                    "max_ms": max(rtts),
                    "mean_ms": statistics.mean(rtts),
                    "median_ms": statistics.median(rtts),
                    "stdev_ms": statistics.stdev(rtts) if len(rtts) > 1 else 0,
                    "packet_loss_pct": (1 - len(self.results)/PACKET_COUNT)*100
                }
            }, f, indent=2)
        print(f"\nResults saved to: {filename}")

class PacketSniffer:
    """Sniff real Phone Bat packets and analyze timing"""

    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((SERVER_IP, LISTEN_PORT))
        self.packets = deque(maxlen=10000)
        self.running = False

    def start(self):
        """Start sniffing"""
        self.running = True
        thread = threading.Thread(target=self._sniff_loop)
        thread.start()
        return thread

    def _sniff_loop(self):
        while self.running:
            try:
                data, addr = self.sock.recvfrom(65536)
                recv_time = time.perf_counter_ns()

                try:
                    packet = json.loads(data.decode('utf-8'))
                    packet["_recv_time_ns"] = recv_time
                    packet["_addr"] = addr
                    self.packets.append(packet)
                except:
                    pass
            except:
                pass

    def stop(self):
        self.running = False

    def analyze(self):
        """Analyze captured packets"""
        if not self.packets:
            print("No packets captured")
            return

        # Group by packet type
        by_type = {}
        for p in self.packets:
            ptype = p.get("header", {}).get("packetType", p.get("type", "unknown"))
            if ptype not in by_type:
                by_type[ptype] = []
            by_type[ptype].append(p)

        print(f"\nCaptured {len(self.packets)} packets")
        for ptype, pkts in by_type.items():
            print(f"  {ptype}: {len(pkts)} packets")

        # Analyze sensor frame rate
        if "sensorFrame" in by_type or 20 in by_type:
            frames = by_type.get("sensorFrame", by_type.get(20, []))
            if len(frames) > 1:
                timestamps = [f.get("header", {}).get("timestamp", 0) for f in frames]
                timestamps = [t for t in timestamps if t > 0]
                if len(timestamps) > 1:
                    intervals = [(timestamps[i] - timestamps[i-1])/1000 for i in range(1, len(timestamps))]
                    print(f"\nSensor frame rate:")
                    print(f"  Mean interval: {statistics.mean(intervals):.2f} ms")
                    print(f"  Rate: {1000/statistics.mean(intervals):.1f} Hz")
                    print(f"  Jitter (stdev): {statistics.stdev(intervals):.2f} ms")

        # Analyze sequence gaps
        for ptype, pkts in by_type.items():
            sequences = []
            for p in pkts:
                seq = p.get("header", {}).get("sequenceNumber", p.get("seq", None))
                if seq is not None:
                    sequences.append(seq)
            if len(sequences) > 1:
                sequences.sort()
                gaps = [sequences[i] - sequences[i-1] for i in range(1, len(sequences))]
                unexpected = [g for g in gaps if g != 1]
                if unexpected:
                    print(f"\n  {ptype}: {len(unexpected)} sequence gaps (expected 1)")
                    print(f"    Gap sizes: {set(unexpected)}")

def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python PhoneBatLatencyTest.py test <phone_ip>     # Run latency test")
        print("  python PhoneBatLatencyTest.py sniff               # Sniff and analyze packets")
        print("  python PhoneBatLatencyTest.py sniff <duration>    # Sniff for N seconds")
        return

    cmd = sys.argv[1]

    if cmd == "test":
        if len(sys.argv) < 3:
            print("Please provide phone IP address")
            return
        phone_ip = sys.argv[2]
        tester = LatencyTester()
        tester.run_test(phone_ip, TEST_PORT)

    elif cmd == "sniff":
        duration = 30
        if len(sys.argv) > 2:
            duration = int(sys.argv[2])

        print(f"Sniffing packets on port {LISTEN_PORT} for {duration} seconds...")
        sniffer = PacketSniffer()
        thread = sniffer.start()

        try:
            time.sleep(duration)
        finally:
            sniffer.stop()
            thread.join(timeout=1.0)
            sniffer.analyze()

    else:
        print(f"Unknown command: {cmd}")

if __name__ == "__main__":
    main()