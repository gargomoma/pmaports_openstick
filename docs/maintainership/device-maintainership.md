# Device Maintainership

Devices are a key concept of postmarketOS. Because the distribution attempts
to support as much hardware as possible, it is obvious that the people who
develop the distribution and those who maintain the devices may be different
people. This means that device maintainers need to understand their
responsibilities and expectations to have a smooth maintainership.

## Expectations

The expectations of a device maintainer are different between categories, with
maintainers of devices in community or main having added responsibilities to
go with the increased quality control of the higher categories.

### All Categories

- Ensure that their device stays working over time
  - It is natural for regressions to occur, but an entirely non-booting
    device is liable to be archived without intervention
- Respond to issues that have to do with the maintained device
  - These can usually be found by either searching the issue tracker for the
    device name or by searching using the device's issue label
- Ensure that the maintained device meets or exceeds the requirements of the
  device category that the device is in
- Respond to merge requests modifying the maintained device package within a
  two week period of the MR's opening
  - This can be as simple as asking for more time to review, but there needs
    to be at least some
    response.
  - See [Approval Rules](../merge-requests/approval-rules.md) for more
    information.
- Follow the general expectations in [maintainership](../maintainership.md)

### Community and Main

- Test the maintained device during the testing period before a distribution
  release and report if their device remains working
  - There will be an issue created during the release period that the
    maintainer can comment in
- Ensure that the device wiki page remains up-to-date and correct
  - This is partially expected of all categories, but is a requirement for
    community and main

## How to Become a Device Maintainer

There are three ways to become a device maintainer. The first is to port a
device, the second is to unarchive a device, and the third is to become a
co-maintainer of an already maintained device.

### Porting a Device

When porting a device, submitters are usually expected to make themselves the
maintainer of the device, with the exception of submissions to the archived
category, and familiarize themselves with the responsibilities that that
entails.

### Unarchiving a Device

When unarchiving a device, submitters are expected to make themselves the
maintainer of the device and fix the backlog of issues that appeared with
the packaging over time.

Most likely, there will be a lot wrong with the package and reviewers will
catch this. This is expected, so do not worry about burdening reviewers.
First, make sure that CI is passing on the merge request, then, go through
the comments reviewers make and fix them. Once everything is in order, the
merge request will most likely be merged and the submitter will be the
maintainer of the package and be pinged on MRs changing it.

### Becoming a Co-Maintainer

This is a more proactive way to become a maintainer. If you own a device
and wish to see it stay supported in postmarketOS, becoming one of its
maintainers is the best way to do this. This usually involves contacting the
existing device maintainer and asking to become a co-maintainer. If they
agree, then make an MR adding yourself and, once it is merged, you will be a
device maintainer.

Co-Maintainers have the same rights as the individual listed in the
`maintainer` variable in a device package's APKBUILD, including requesting
changes to MRs and patches without another maintainer's support. Make sure to
communicate with the other maintainers of your device to have an agreement on
what changes need to be made and how to test those changes.
