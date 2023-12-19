# TCP-over-UDP-implementation

UIUC ECE438 MP2

## How to use
### Commands:
./sender receiver_hostname receiver_port filename_to_xfer bytes_to_xfer

./receiver UDP_port filename_to_write

### How to limit network performance:
You can use the same basic test environment described above. However, the network performance will be ridiculously good (same goes for testing on localhost), so you’ll need to limit it.

If your network interface inside the VM is eth0, then run (from inside the VM) the following command:

**sudo tc qdisc del dev eth0 root 2>/dev/null**

to delete existing tc rules. Then use,

**sudo tc qdisc add dev eth0 root handle 1:0 netem delay 20ms loss 5%**

followed by

**sudo tc qdisc add dev eth0 parent 1:1 handle 10: tbf rate 20Mbit burst 10mb latency 1ms**

will give you a 20Mbit, 20ms RTT link where every packet sent has a 5% chance to get dropped. Simply omit the loss n% part to get a channel without artificial drops.

(You can run these commands just on the sender; running them on the receiver as well won’t make much of a difference, although you’ll get a 20ms RTT if you don’t adjust the delay to account for the fact that it gets applied twice.)

## Components of TCP
- RTT Estimation & Timeout
- Reliable Data Transfer
- Congestion Control
- Connection Management

### RTT Estimation & Timeout: 
based on RFC6289

### Reliable Data Transfer: 
TCP Segment Structure

### Congestion Control: 
CWND
<img width="1039" alt="截屏2023-11-20 下午5 41 10" src="https://github.com/emmaleee789/TCP-over-UDP-implementation/assets/112675973/89fe9d14-c5e5-4d2e-ad61-8bb32776a4a4">

### Connection Management: 
TCP 3-way handshake & 4-way handshake
