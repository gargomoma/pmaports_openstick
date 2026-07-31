# Maintainership

Maintainers are a core part of postmarketOS. Without them, there would be no
distro to run. Because of this, good maintainership is the key to getting
changes done and keeping software functioning.

People generally overestimate the effort required to be a package maintainer.
The general expectations are:

- Test the package occasionally to make sure that it stays working
- Respond to issues and merge requests about the package
  - For MRs, maintainers should comment "LGTM" to approve them if said
    maintainer does not have approval rights.
  - MRs must be responded to in some form within two weeks of either opening
    or undrafting. See (Approval Rules)[../merge-requests/approval-rules.md]
    for more information.
- Ensure that the package stays working across distribution releases
- Ensure that the contact information in the maintainer fields are correct

There may be more package-specific expectations. These are elaborated upon in
the subpages.

## Don't Maintain Alone

The general best practice for maintainership is to not do it alone. A
package that has more than one maintainer is much easier to handle for both
the maintainers and the people who may contribute to the package. More
maintainers means a faster response time and, if a maintainer quits the
project, less of a chance for the package to be archived or deleted.

### High-Level Process to Become a Co-Maintainer

The general process to become a co-maintainer includes:

- Contact the current maintainers to ask to become a co-maintainer
  - This can be done either in private or in a comment of an open MR the person
    looking to become a co-maintainer makes to the package they wish to a
    become co-maintainer of.
- Get unanimous current package maintainer approval
- Create an MR adding a co-maintainer line above the current maintainers
  - See [the packaging documentation](packaging.md) for the exact format.
- Get the MR approved by the current package maintainers and merged

See the subpages for more specific information pertaining to the type of package
a contributor withes to maintain.

### Finding Co-Maintainers

If you are already a package maintainer and are looking for co-maintainers,
ask in public rooms or contact contributors you think would be good
co-maintainers. These are commonly active contributors to a certain package
or known users of a device who are active in the community and willing to help
debug issues that arise with a device.

## Types of Packages

There are three types of packages, which each come with their own expectations
of maintainer conduct.

- Device Packages
  - Packages that represent a device. These include needed configuration for a
    device to function properly and is what pmbootstrap uses to check if a
    device exists.
- Kernel Packages
  - Packages that package the Linux kernel. These can be device-specific or
    generic close-to-mainline kernels.
- Software Packages
  - Packages that contain general software. These are usually what someone
    thinks of when they think of "packages".
  - This category also includes packages that may be related to a specific
    device or SOC family, including SOC-specific packages and device firmware
    packages.

There are most likely other sub-categories that can be thought of, but these
are the main three in regards to maintainership expectations.

See the subpages for how to maintain a package depending on the category.

```{eval-rst}
.. toctree::
   :hidden:

   maintainership/device-maintainership
   maintainership/kernel-maintainership
   maintainership/software-maintainership
```
