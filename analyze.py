import sys
import pandas as pd
import numpy as np
from pathlib import Path

def format_latency(us):
    if np.isnan(us): return "N/A"
    if us < 1000: return f"{us:.1f} μs"
    if us < 1e6: return f"{us/1000:.3f} ms"
    return f"{us/1e6:.6f} s"

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 analyze.py <sender.csv> <receiver.csv>")
        sys.exit(1)
    sfile = Path(sys.argv[1])
    rfile = Path(sys.argv[2])
    if not sfile.exists() or not rfile.exists():
        print("Error: missing file(s).")
        sys.exit(1)

    sender = pd.read_csv(sfile)
    receiver = pd.read_csv(rfile)
    if 'seq' not in receiver.columns:
        raise SystemExit("Receiver CSV missing 'seq' column")
    receiver = receiver.sort_values(['seq', 'recv_ts_ns' if 'recv_ts_ns' in receiver.columns else receiver.columns[1]])
    receiver = receiver.drop_duplicates(subset=['seq'], keep='first').reset_index(drop=True)

    if 'seq' not in sender.columns:
        raise SystemExit("Sender CSV missing 'seq' column")
    sender = sender.drop_duplicates(subset=['seq'], keep='first').reset_index(drop=True)

    total_sent = len(sender)
    total_recv_unique = len(receiver)

    # safe merge on seq
    df = pd.merge(sender, receiver[['seq','recv_ts_ns']], on='seq', how='outer', suffixes=('_send','_recv'))
    if 'send_ts_ns' in sender.columns and 'recv_ts_ns' in receiver.columns:
        df = pd.merge(sender[['seq','send_ts_ns']], receiver[['seq','recv_ts_ns']], on='seq', how='outer')
        df['oneway_us'] = (df['recv_ts_ns'] - df['send_ts_ns']) / 1000.0
        df.loc[df['oneway_us'] < 0, 'oneway_us'] = np.nan
    else:
        df['oneway_us'] = np.nan
    if 'ack_recv_ts_ns' in sender.columns:
        sender['rtt_us'] = (sender['ack_recv_ts_ns'] - sender['send_ts_ns']) / 1000.0
        sender.loc[sender['rtt_us'] <= 0, 'rtt_us'] = np.nan
    else:
        sender['rtt_us'] = np.nan

    retransmits = sender['retransmits'].sum() if 'retransmits' in sender.columns else 0
    retransmit_rate = (retransmits / total_sent)*100 if total_sent>0 else 0.0

    print("\nMessage Statistics:")
    print(f"  Total messages sent: {total_sent:,}")
    print(f"  Total retransmissions: {retransmits:,} ({retransmit_rate:.2f}%)")
    print(f"  Unique messages received: {total_recv_unique:,}")
    loss_rate = (1.0 - (total_recv_unique / total_sent)) * 100 if total_sent>0 else 0.0
    print(f"  Packet loss rate (unique recv): {loss_rate:.4f}%")

    # Latency stats
    if df['oneway_us'].notna().sum() > 0:
        a = df['oneway_us'].dropna().values
        def pct(x,p): return np.nanpercentile(x,p)
        print("\nOne-way latency (sender -> receiver):")
        print(f"  Samples: {len(a):,}")
        print(f"  Min: {format_latency(np.nanmin(a))}")
        print(f"  Median (p50): {format_latency(pct(a,50))}")
        print(f"  Mean: {format_latency(np.nanmean(a))}")
        print(f"  p90: {format_latency(pct(a,90))}")
        print(f"  p95: {format_latency(pct(a,95))}")
        print(f"  p99: {format_latency(pct(a,99))}")
        print(f"  p99.9: {format_latency(pct(a,99.9))}")
        print(f"  Max: {format_latency(np.nanmax(a))}")

    # RTT stats
    if sender['rtt_us'].notna().sum() > 0:
        b = sender['rtt_us'].dropna().values
        print("\nRTT (sender):")
        print(f"  Samples: {len(b):,}")
        print(f"  Median (p50): {format_latency(np.nanpercentile(b,50))}")
        print(f"  p99: {format_latency(np.nanpercentile(b,99))}")
        print(f"  Max: {format_latency(np.nanmax(b))}")

    # Throughput based on receiver timestamps (unique)
    if 'recv_ts_ns' in receiver.columns and len(receiver) > 1:
        t0 = receiver['recv_ts_ns'].min()
        t1 = receiver['recv_ts_ns'].max()
        dur_s = (t1 - t0) / 1e9
        if dur_s > 0:
            throughput = len(receiver) / dur_s
            print("\nThroughput:")
            print(f"  Average: {throughput:.0f} msgs/sec")
            print(f"  Duration: {dur_s:.3f} s")

if __name__ == "__main__":
    main()
