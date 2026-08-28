# Vendored: ptpd (github.com/ptpd/ptpd)

License: see COPYRIGHT (BSD-style). Unmodified upstream `src/`.

The full ptpd daemon targets POSIX (sockets with SO_TIMESTAMPING, adjtimex,
syslog, ifaddrs, getopt, an ini config parser, SNMP, an NTP engine) and does not
run bare-metal. FlintRTOS therefore REUSES ptpd's OS-independent IEEE-1588 core:
  - ptp_primitives.h, ptp_datatypes.h, constants.h, and the def/ field tree
    (the exact on-wire message data model), and
  - the TimeInternal arithmetic from arith.c (reimplemented in port/ptp/ptp_time.c).
The FlintRTOS `dep/` layer (UDP over lwIP, a disciplined clock + PI servo, and the
ordinary-clock slave state machine) is under `port/ptp/`.
