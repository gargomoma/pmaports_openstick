# Software Maintainership

Software packages are the most common type of packages for most distributions.
These type of packages include regular files that serve no unique purpose to
postmarketOS.

There are three types of software packages:

- Normal
- Forked
- Temporary

## Maintaining Normal Software Packages

Normal software packages are regular packages. They are stored in `main/` and
follow the same packaging process as Alpine Linux's aports. Generally, the
only ways to become a maintainer of a normal software package is to either
submit it or become a co-maintainer.

This package type can also include specific types of packages in `device/`,
including SOC-specific packages and firmware packages.

Normal software packages follow the general expectations set in
[maintainership](../maintainership.md).

## Maintaining Forked Software Packages

Forked software packages are normal packages from Alpine's aports that were
forked to pmaports for some purpose. These come in two forms. The first is
packages in `temp/`, which can be forked for any number of purposes, and the
second is packages in `extra-repos/systemd/`, which are forked specifically to
build with systemd. Generally, forked packages only need one maintainer, as
the changes made to them come from Alpine.

### Expectations

- Respond to issues and MRs to do with the package, redirecting them upstream
  to Alpine if it isn't postmarketOS-specific
- Keep the package versioning and logic in sync with what is in aports
- Follow the general expectations in [maintainership](../maintainership.md)

## Maintaining Temporary Software Packages

Temporary software packages are special in that they are added for the
specific purpose of being removed in the future. These are only stored in
`temp/` and usually have a line at the top explaining why they are temporary.

These packages generally are either only packages that are waiting for
acceptance in aports or to have changes accepted to another upstream but are
explicitly necessary in postmarketOS or are workaround packages that are
planned to be removed in the future.

To become a maintainer of these packages, either submit a package or attempt
to become a co-maintainer. Some temporary packages may not make sense to have
more than one maintainer, but others may need them.

### Expectations

- Keep track of upstreaming process or the requirement for removal
- Cleanly remove the package whenever the requirement for removal is completed
- Follow the general expectations in [maintainership](../maintainership.md)
