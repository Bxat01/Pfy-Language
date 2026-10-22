# Publishing Pfy and its editor support

This guide distinguishes releases from global language registration. A GitHub
Release distributes files; a VS Code Marketplace listing installs editor
support; GitHub Linguist and GitLab have separate language-detection systems.
Publishing one does not automatically publish the others.

## Current project status

- The repository is private. Only users with access can view its source and
  download GitHub Release assets.
- Release `v1.0.0` contains the Windows x64 CLI archive and a VS Code `.vsix`.
- The VS Code extension is not yet listed on the public Marketplace.
- The GHCR publishing workflow exists, but its runs have failed with
  `startup_failure` before any build job started. Do not advertise the
  container image as published until a workflow succeeds and the package is
  visible.
- GitHub currently counts the project as C++ and CMake, not as a registered
  Pfy language.

## Make the source publicly available

If the goal is open-source adoption, make the repository public only after
checking that it contains no secrets, private data, or files that must remain
private:

1. Open the repository's **Settings** on GitHub.
2. Under **General**, find **Danger Zone** → **Change repository visibility**.
3. Choose **Make public** and complete GitHub's confirmation.
4. Check the public repository and release pages in a signed-out browser.

The project uses the MIT license. Keep the root `LICENSE` file in the
repository and include it in distributed packages.

## Publish a normal GitHub Release

1. Update the project version in `CMakeLists.txt`, the CLI version if needed,
   the VS Code extension's `package.json`, and the changelog.
2. Build and run the CMake/CTest checks on supported platforms.
3. Prepare release assets and calculate checksums.
4. Create and push an annotated version tag, for example:

   ```sh
   git tag -a v1.0.1 -m "Pfy 1.0.1"
   git push origin v1.0.1
   ```

5. Open the repository's **Releases** page, create a release for that tag, add
   concise notes, and attach tested assets.
6. Verify that the assets can be downloaded by the intended audience.

Official documentation:
[About releases](https://docs.github.com/repositories/releasing-projects-on-github/about-releases)
and
[Managing releases](https://docs.github.com/repositories/releasing-projects-on-github/managing-releases-in-a-repository).

## Publish the VS Code extension

The repository already contains an extension package in `vscode-pfy/`. Before
publishing it publicly:

1. Ensure the publisher ID `bxat01` belongs to the account you control. If it
   does not, create a Marketplace publisher and update `publisher` in
   `vscode-pfy/package.json`.
2. Update the extension version and changelog for each release.
3. Package and test the VSIX:

   ```sh
   cd vscode-pfy
   npx @vscode/vsce package
   ```

4. Install the generated `.vsix` in a clean VS Code profile and test a `.pfy`
   file.
5. Follow Microsoft's
   [extension publishing guide](https://code.visualstudio.com/api/working-with-extensions/publishing-extension)
   to create or sign in to the publisher, authenticate `vsce`, and publish.
6. Verify the listing on the
   [Visual Studio Marketplace](https://marketplace.visualstudio.com/vscode).

Publishing requires Marketplace publisher access and credentials. Never commit
personal access tokens or publishing credentials to this repository. The
current `.vsix` is a GitHub Release asset, not a Marketplace listing.

## Request GitHub language recognition

GitHub language statistics and highlighting are maintained by
[GitHub Linguist](https://github.com/github-linguist/linguist), not by the
repository owner alone. To request a first-class Pfy definition:

1. Read Linguist's current
   [contribution requirements](https://github.com/github-linguist/linguist/blob/main/CONTRIBUTING.md).
2. Confirm that the language and `.pfy` extension meet the current usage and
   maintenance criteria. A new-language pull request may be declined if usage
   is too small.
3. Prepare a concise language definition and a representative sample. The
   sample should be real Pfy source, not just a tiny one-line fixture.
4. Add the grammar, tests, and any required documentation in a pull request to
   Linguist, following its repository instructions.
5. Wait for Linguist review and merge. GitHub will use the new definition
   after it is released and deployed.

Do not label Pfy as CMake or C++ just to change the language bar. A
`.gitattributes` override only helps when Linguist already knows the language;
it cannot register a new language globally.

## GitLab and other code hosts

GitHub Linguist changes do not register Pfy on GitLab or other hosting
services. Each service controls its own language catalog, detection rules, and
highlighting support. Check that host's current contribution process before
requesting a language entry. For GitLab, start with its
[repository file documentation](https://docs.gitlab.com/user/project/repository/files/);
there is no universal per-project switch that guarantees Pfy syntax
highlighting on all GitLab instances.

## Support more editors

Editor support is a collection of separate integrations:

- Distribute the VS Code extension through its Marketplace.
- Reuse the TextMate grammar in editors that support compatible TextMate
  grammars.
- Create and maintain native packages for editors that need their own format.
- Consider Tree-sitter for parsers and editors that support it.
- Build an LSP server if cross-editor completion, diagnostics, or navigation
  are required.

See [Editor support](editor-support.md) for the current feature list and
what syntax highlighting does and does not provide.

## Container package

The repository includes a Dockerfile and a GitHub Actions workflow for
publishing an image to GitHub Container Registry (GHCR). A version tag alone
does not prove that a package was created. Before announcing an image:

1. Open **Actions** and verify that **Publish container** completed
   successfully for the version tag.
2. Open the repository owner's **Packages** page and confirm the image and
   version tag exist.
3. Test pulling and running the image from a clean environment.
4. Review package visibility and link the container package to this repository.

See GitHub's documentation on
[working with the Container registry](https://docs.github.com/packages/working-with-a-github-packages-registry/working-with-the-container-registry).
