# Coaster Manager v1.1.5 — one-time release upload

Drop the **unextracted** `Coaster-Manager-v1.1.5-GitHub-Ready.zip` file into this directory and commit it on `main`.

The `publish-v1.1.5.yml` GitHub Actions workflow will automatically:

1. Verify the Windows DLL matches the SHA-256 of the tested v1.1.5 build.
2. Import the actual source files, tests, scripts and DLL into the repository.
3. Commit the source and remove the temporary upload ZIP from `incoming/`.
4. Create **GitHub Release v1.1.5** and attach `Coaster-Manager-v1.1.5-Workshop-Upload.zip` containing the verified DLL under `DLL Mod/`.

You should **not** upload the unextracted GitHub-ready ZIP to Steam Workshop. Use the Workshop release asset instead.

If the action fails due to restricted repository Actions permissions, check **Settings → Actions → General → Workflow permissions** for `Read and write permissions`, then re-run the failed workflow. If the action is disabled, use the GitHub web interface to create a release manually.
