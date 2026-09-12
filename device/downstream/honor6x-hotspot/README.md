# Honor 6X hotspot package

This package is the disabled-by-default P2P-GO/NAT component for the Honor 6X
downstream port. It owns only the temporary hotspot lifecycle. The
normal station Wi-Fi service remains the recovery path and is restored after a
clean stop or a failed start.

NAT and forwarding use isolated iptables-legacy chains that are removed on stop
or failed startup, so the controller does not leave rules in the system
firewall. The Honor 6X 4.4 downstream kernel exposes the legacy NAT path; its
nft masquerade path does not translate client traffic reliably.
Preflight checks the complete activation transaction before stopping station
Wi-Fi; a table-only check is not sufficient. Both NAT hooks are registered for
the Linux 4.4 reply path. On kernels before 4.18, a registered legacy iptables
NAT table blocks startup: this package never flushes foreign rules, unloads
modules or silently switches firewall backends. This guard does not replace
the final kernel/backend compatibility review or a real-client traffic test.

Offline package tests cover controller lifecycle and NAT preflight, including
failure without changing station Wi-Fi or enabling IPv4 forwarding. All
service, network and firewall commands in those tests are isolated mocks.

The package does not replace the system `wpa_supplicant`. Its P2P helper is the
separately packaged `wpa-supplicant-honor6x-p2p` binary. No boot service is
installed and no Wi-Fi credentials are included.

The standard Honor 6X image includes this package while keeping its service
off by default. Distribution support is complete only after a clean-image
acceptance proves start, client DHCP/internet, stop, failed-start rollback and
reboot recovery.
