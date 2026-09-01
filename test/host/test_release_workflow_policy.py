from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ReleaseWorkflowPolicyTest(unittest.TestCase):
    def read(self, relative_path: str) -> str:
        return (ROOT / relative_path).read_text(encoding="utf-8")

    def test_release_version_is_validated_before_building_assets(self) -> None:
        workflow = self.read(".github/workflows/release.yml")
        update_step = workflow.index("- name: Update firmware version and changelog")
        validation_step = workflow.index(
            "- name: Validate release version against source"
        )
        build_step = workflow.index("- name: Build New Design WebUI")

        self.assertLess(update_step, validation_step)
        self.assertLess(validation_step, build_step)
        self.assertIn("SOURCE_VERSION=\"$(tr -d '\\r\\n' < version.txt)\"", workflow)
        self.assertIn(
            'if [ "$SOURCE_VERSION" != "$RELEASE_VERSION" ]; then', workflow
        )
        self.assertIn("exit 1", workflow[validation_step:build_step])

    def test_contribution_guide_uses_the_default_branch(self) -> None:
        guide = self.read("CONTRIBUTING.md")
        self.assertNotIn("master", guide)
        self.assertIn("git checkout main", guide)
        self.assertIn("git pull upstream main", guide)
        self.assertIn("git rebase main", guide)
        self.assertIn("merged into main", guide)

    def test_release_documentation_explains_tag_validation(self) -> None:
        versioning = self.read("docs/RELEASE_VERSIONING.md")
        self.assertIn("Prefer a manual workflow dispatch", versioning)
        self.assertIn("version.txt", versioning)
        self.assertIn("workflow stops before", versioning)

        guidance = self.read("CLAUDE.md")
        self.assertIn("Prefer a manual dispatch", guidance)
        self.assertIn("version.txt", guidance)
        self.assertIn("workflow fails", guidance)


if __name__ == "__main__":
    unittest.main()
