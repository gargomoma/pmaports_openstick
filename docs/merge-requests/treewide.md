# Tree-wide Merge Requests

Tree-wide merge requests that touch several packages at once are discouraged.
Instead, the preferential route to make tree-wide changes in pmaports is to
notify the maintainers of packages that need to be changed and then wait for
the maintainer to create a merge request to make the necessary adjustments.

This is contrary to previous practice, where large packaging refactors and
cleanups were done in tree-wide merge requests and merged without approval of
all package maintainers. However, this approach has several downsides:

* Every once in a while, a change in a treewide broke certain functionality
  because it was not tested sufficiently or slipped through review

* The use of tree-wide MRs for packaging cleanup and enforcement of policies
  without maintainer involvement has created the expectation that such cleanups
  are done by the postmarketOS team and not the responsibility of the package
  maintainers and/or done without their involvement

* Large, tree-wide merge requests are unlikely to be reviewed with the same
  scrutiny as merge requests that would only touch a single package, making it
  more likely that mistakes slip through

Letting the maintainers of the packages perform the changes has several upsides:

* The changes are smaller in scope, easy to review and tested by the maintainer

* Maintainers are more likely to be aware of postmarketOS packaging policies and
  changes to these policies if they're the ones who have to make the adjustments

* If the changes are not performed after the maintainer is informed that their
  package does not meet our guidelines, the team can start the process for
  finding a new maintainer to ensure the package is actively maintained

In certain cases, tree-wide MRs are still the preferred way to go about things,
but they should only be an absolute last resort. The changes that make sense to
perform in tree-wide MRs for example include changes that can be fully automated
to be able to perform a refactor without a period of retaining backwards
compatibility. Most adjustments however, like packaging refactors, can and should
be done on a per-package basis.
