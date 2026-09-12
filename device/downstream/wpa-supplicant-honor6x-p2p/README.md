# Honor 6X existing-P2P management probe

This package builds a second, isolated `wpa_supplicant` binary. It does not
replace Alpine's `/sbin/wpa_supplicant` and is not enabled at boot.

## First revision rejected

The first authorized attach-only probe rejected revision `2.11-r0`. Standard Wi-Fi
and internet recovered, but `p2p0` changed from `P2P-device` to `managed`.
Do not install the retained `2.11-r0` APK.

Two source-level causes are confirmed:

- A `p2p_mgmt` interface without command-line `-C` has the control interface
  read from its configuration file cleared by `wpa_supplicant_init_iface()`.
  The expected control socket therefore cannot appear.
- nl80211 treats an existing non-dynamic interface as a station during driver
  initialization. It calls `set_mode(STATION)` before the later
  `p2p_mgmt` flag can affect supplicant setup. This changed the vendor-created
  `p2p0` and the normal deinit path did not restore its original type.

A replacement must explicitly preserve an externally owned P2P Device, avoid
deleting it during deinit, retain failure logs, and pass the control directory
through `-C`. Merely adding `-C` to the first revision is insufficient.

## Second revision rejected

Revision `2.11-r1` adds `0005-nl80211-preserve-external-p2p-device.patch`.
Its authorized attach-only probe failed. The vendor driver's
`start_p2p_device` callback is an unimplemented stub that always returns an
error. A malformed hunk header in the original `0005` also caused Alpine's
`patch` utility to apply the detection changes while silently omitting the
deinit guard. Failure cleanup then issued `DEL_INTERFACE`, and the vendor
driver asynchronously removed `p2p0`. Standard Wi-Fi did not recover during
the bounded cleanup; the user subsequently rebooted the phone and restored
the baseline. The exact malformed patch is retained under
`../evidence/0005-r1-malformed.patch`.

Do not install or run the retained `2.11-r1` APK.

## Third offline candidate

Revision `2.11-r2` repairs the patch series and adds
`0006-nl80211-skip-external-p2p-lifecycle.patch`. For a pre-existing,
non-dynamic P2P Device only, nl80211 now skips generic start, stop, and delete
operations. Dynamically created P2P Devices retain their normal lifecycle.

The complete series passes `git apply --check` and builds successfully for
aarch64 in the v26.06 qemu-only buildroot. The APK contains only the isolated
`/usr/libexec/honor6x/wpa_supplicant-p2p` payload. One authorized v3
attach-only probe passed: both control sockets responded, `p2p0` retained its
P2P-device type, upstream Wi-Fi stayed online, and standard Wi-Fi recovered
cleanly afterward. The temporary phone-side binary and validator were removed
after evidence capture; nothing was installed or enabled at boot.

The validation script now requires the new literal argument
`--authorized-attach-only-v2`, supplies the control directory with `-C`, and
retains non-secret interface, address, route, and process logs under
`/tmp/honor6x-p2p-attach-v2-last` before restoration. It still contains no P2P
discovery, group creation, DHCP-server, forwarding, NAT, or VPN commands.

The `2.11-r2` APK must not be installed or run without a new explicit
authorization and a fresh baseline check.

The `-Q` option marks the current command-line interface as an already existing
dedicated P2P Device management interface. On the Honor 6X, this lets a bounded
probe attach to the vendor-created `p2p0` without issuing the P2P Device add
request rejected by `07-fix-p2p.patch`.

This does not prove that P2P-GO lifecycle operations are safe. No discovery,
group creation, DHCP server, NAT, forwarding, or VPN routing was attempted.
Never remove `07-fix-p2p.patch` as part of this experiment.

The v26.06 build must use the APKBUILD's `!pmb:crossdirect` option. The current
crossdirect environment compiles all objects but cannot complete the GCC final
link; qemu-only produces the reviewed AArch64 package without changing source
behavior. The built APK is retained in `../artifacts/`.

`../honor6x-p2p-attach-validation` is hard-disabled because it is the rejected
revision-2 procedure. `../honor6x-p2p-attach-validation-v3` is the reviewed r2
attach-only procedure and requires the literal `--authorized-attach-only-v3`
gate. It contains no P2P discovery, group creation, DHCP-server, forwarding,
NAT, VPN, reboot, or flash operation. Both earlier failures remain documented
here and in the hotspot capability audit.


