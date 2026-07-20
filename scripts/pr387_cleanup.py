def apply():
    # Workflow and archive cleanup is committed separately with the maintainer
    # token. GitHub Actions may not rewrite workflow files with GITHUB_TOKEN.
    # Keeping this module as a no-op lets the guarded source refactor commit all
    # firmware/WebUI changes without requesting workflow permissions.
    return
