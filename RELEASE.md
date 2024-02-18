# Releasing

This describes for the maintainers of RGBDS how to publish a new release on
GitHub.

1. Update, commit, and push [include/version.hpp](include/version.hpp) with
   values for `PACKAGE_VERSION_MAJOR`, `PACKAGE_VERSION_MINOR`,
   `PACKAGE_VERSION_PATCH`, and `PACKAGE_VERSION_RC`, as well as
   [Dockerfile](Dockerfile) with a value for `ARG version`. Only define
   `PACKAGE_VERSION_RC` if you are publishing a release candidate! You can
   use <code>git commit -m "Release <i>&lt;version&gt;</i>"</code> and
   `git push origin master`.

2. Create a Git tag formatted as <code>v<i>&lt;MAJOR&gt;</i>.<i>&lt;MINOR&gt;</i>.<i>&lt;PATCH&gt;</i></code>,
   or <code>v<i>&lt;MAJOR&gt;</i>.<i>&lt;MINOR&gt;</i>.<i>&lt;PATCH&gt;</i>-rc<i>&lt;RC&gt;</i></code>
   for a release candidate. <code><i>MAJOR</i></code>, <code><i>MINOR</i></code>,
   <code><i>PATCH</i></code>, and <code><i>RC</i></code> should match their values from
   [include/version.hpp](include/version.hpp). You can use <code>git tag <i>&lt;tag&gt;</i></code>.

3. Push the tag to GitHub. You can use <code>git push origin <i>&lt;tag&gt;</i></code>.

   GitHub Actions will run the [create-release-artifacts.yaml](.github/workflows/create-release-artifacts.yaml)
   workflow to detect the tag starting with "`v[0-9]`" and automatically do the following:

   1. Build 32-bit and 64-bit RGBDS binaries for Windows with `cmake`.

   2. Package the binaries into zip files.

   3. Package the source code into a tar.gz file with `make dist`.

   4. Create a draft GitHub release for the tag, attaching the three
      packaged files. It will be a prerelease if the tag contains "`-rc`".

      If an error occurred in the above steps, delete the tag and restart the
      procedure. You can use <code>git push --delete origin <i>&lt;tag&gt;</i></code> and
      <code>git tag --delete <i>&lt;tag&gt;</i></code>.

4. GitHub Actions will run the [create-release-docs.yml](.github/workflows/create-release-docs.yml)
   workflow to add the release documentation to [rgbds-www](https://github.com/gbdev/rgbds-www).

   For a release candidate, which creates a prerelease, you will have to
   take these steps yourself.

   1. Clone [rgbds-www](https://github.com/gbdev/rgbds-www). You can use
      `git clone https://github.com/gbdev/rgbds-www.git`.

   2. Make sure that you have installed `groff` and `mandoc`. You will
      need `mandoc` 1.14.5 or later to support `-O toc`.

   3. Run <code>.github/actions/get-pages.sh -r <i>&lt;path/to/rgbds-www&gt;</i> <i>&lt;tag&gt;</i></code>.
      This will render the RGBDS documentation as HTML and PDF and copy it to
      `rgbds-www`.

      If you do not have `groff` installed, you can change
      `groff -Tpdf -mdoc -wall` to `mandoc -Tpdf -I os=Linux` in
      [.github/actions/get-pages.sh](.github/actions/get-pages.sh) and it
      will suffice.

   4. Commit and push the documentation. You can use <code>git commit -m
      "Create RGBDS <i>&lt;tag&gt;</i> documentation"</code> and `git push origin master`
      (within the `rgbds-www` directory, not RGBDS).

5. Write a changelog in the GitHub draft release.

6. Click the "Publish release" button to publish it!

7. Update the `release` branch. You can use `git push origin release`.
