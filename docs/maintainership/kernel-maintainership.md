# Kernel Maintainership

Kernel packages are unique in postmarketOS. Unlike most Linux distributions,
which have one or a select few kernel packages that support all devices,
postmarketOS has a lot of device or SOC-specific kernels. While the main goal
is for every device to use the main kernel packages, most devices cannot do
this, so the distribution supports many kernels instead.

## Expectations

- Keep the kernel within the requirements of the device category that is in
  - This may include keeping the kernel-version up-to-date or building with
    LLVM. These requirements may change, but maintainers will be notified and
    given a grace period to make changes if this occurs.
  - These requirements can be found in
    [device categorization](../packaging/device-categorization).
- Keep the kernel's kconfig in sync with kconfigcheck.toml
  - This is done by running `pmbootstrap kconfig check <kernelname>` and
    fixing any issues that arise. If there is a problem that prevents syncing
    the kernel config, maintainers should open an issue in pmaports.
- Follow the general expectations in [maintainership](../maintainership.md)

## How to Become a Kernel Maintainer

There are three ways to become a kernel maintainer. The first is to port a
device, the second is to unarchive a kernel, and the third is to become a
co-maintainer of an already maintained kernel.

### Porting a Device

If a new device is submitted alongside a new kernel, the submitter is expected
to become the maintainer of that new kernel. It is also recommended that
submitters of new devices using existing kernels consider becoming a
maintainer of the kernel.

### Unarchiving a Kernel

When unarchiving a kernel, submitters are expected to make themselves the
maintainer of the kernel and fix the backlog of issues that appeared with
the packaging over time.

These issues will most likely be small packaging or style issues, but could
also include changes in the device categorization that the unarchived package
does not reflect. An example of this would be building with LLVM being a
requirement for testing and above.

### Becoming a Co-Maintainer

This is a more proactive way to become a maintainer. If you own a device and
wish to see it get kernel versions earlier or have kconfig changes made
sooner, consider becoming a kernel maintainer. This requires discussion with
the existing maintainers, but makes things easier for both the current
maintainers and the potential future patch submitters.
