# Contributing to HB-RF-ETH-ng

First off, thank you for considering contributing to HB-RF-ETH-ng! It's people like you that make HB-RF-ETH-ng such a great tool.

## Where do I go from here?

If you've noticed a bug or have a feature request, make one! It's generally best if you get confirmation of your bug or approval for your feature request this way before starting to code.

## Fork & create a branch

If this is something you think you can fix, then fork HB-RF-ETH-ng and create a branch with a descriptive name.

A good branch name would be (where issue #325 is the ticket you're working on):

```sh
git checkout -b 325-add-new-feature
```

## Get the test suite running

Make sure you have all the necessary dependencies installed and that you can build the firmware and run the tests. Refer to the documentation in the repository for instructions on how to set up your development environment.

## Implement your fix or feature

At this point, you're ready to make your changes. Feel free to ask for help; everyone is a beginner at first :smile_cat:

## Make a Pull Request

At this point, you should switch back to your main branch and make sure it's up to date with HB-RF-ETH-ng's main branch:

```sh
git remote add upstream https://github.com/Xerolux/HB-RF-ETH-ng.git
git checkout main
git pull upstream main
```

Then update your feature branch from your local copy of main, and push it!

```sh
git checkout 325-add-new-feature
git rebase main
git push --set-upstream origin 325-add-new-feature
```

Finally, go to GitHub and make a Pull Request :D

## Keeping your Pull Request updated

If a maintainer asks you to "rebase" your PR, they're saying that a lot of code has changed, and that you need to update your branch so it's easier to merge.

## Merging a PR (maintainers only)

A PR can only be merged into main by a maintainer if:

* It is passing CI.
* Its diff has been reviewed by a maintainer. Trusted automated dependency
  updates may be merged by a maintainer after the diff and all required checks
  have been reviewed.
* It has no requested changes.
* It is up to date with current main.

Any maintainer is allowed to merge a PR if all of these conditions are met.
