# GitHub Actions workflows

This directory contains a number of workflows for use with [GitHub Actions](https://docs.github.com/actions).  They specify what standards should be expected for development of this software, including pull requests.  These workflows are designed to work out of the box for any research software prototype, especially those based on [Qiskit](https://qiskit.org/).

## Lint check (`lint.yml`)

This workflow checks that the code is formatted properly and follows the style guide (`clang-format` and `clang-tidy`).

## Latest version tests (`test_latest_versions.yml`)

This workflow installs the latest version of tox and runs the current repository's tests.

## Documentation (`docs.yml`)

This workflow ensures that the [Sphinx](https://www.sphinx-doc.org/) documentation builds successfully.  It also publishes the resulting build to [GitHub Pages](https://pages.github.com/) if it is from the appropriate branch (e.g., `main`).

## Citation preview (`citation.yml`)

This workflow is only triggered when the `CITATION.bib` file is changed.  It ensures that the file contains only ASCII characters ([escaped codes](https://en.wikibooks.org/wiki/LaTeX/Special_Characters#Escaped_codes) are preferred, as then the `bib` file will work even when `inputenc` is not used).  It also compiles a sample LaTeX document which includes the citation in its bibliography and uploads the resulting PDF as an artifact so it can be previewed (e.g., before merging a pull request).

## Release (`release.yml`)

This workflow is triggered by a maintainer pushing a tag that represents a release.  It publishes the release to github.com.
