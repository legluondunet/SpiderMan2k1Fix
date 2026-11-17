"""Safety contracts for the rate-limited GitHub Actions storage cleaner."""
import importlib.util
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/prune_actions_history.py"
WORKFLOW = ROOT / ".github/workflows/spiderman2k1fix-actions-cleanup.yml"
spec = importlib.util.spec_from_file_location("spider_actions_cleanup", SCRIPT)
cleaner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cleaner)


class FakeAPI:
    def __init__(self):
        self.deleted = []
        self.runs = [
            {"id": 222, "status": "in_progress", "event": "push"},
            {"id": 220, "status": "completed", "event": "push"},
            {"id": 219, "status": "completed", "event": "push"},
            {"id": 218, "status": "completed", "event": "pull_request"},
        ]
        self.artifacts = [
            {"id": 301, "name": "SpiderMan2k1Fix", "expired": False,
             "workflow_run": {"id": 220}},
            {"id": 302, "workflow_run": {"id": 219}},
            {"id": 303, "workflow_run": {"id": 222}},
        ]
        self.caches = [
            {"id": 401, "key": "spider-unified-internal-219-1"},
            {"id": 402, "key": "spider-unified-internal-220-1"},
            {"id": 403, "key": "unrelated-toolchain"},
        ]

    def get(self, path):
        if path.startswith("/actions/workflows/"):
            return {"workflow_runs": self.runs}
        if path.startswith("/actions/runs/") and "/artifacts?" in path:
            run_id = int(path.split("/")[3])
            return {"artifacts": [
                artifact for artifact in self.artifacts
                if artifact.get("workflow_run", {}).get("id") == run_id
            ]}
        if path.startswith("/actions/artifacts"):
            return {"artifacts": self.artifacts}
        if path.startswith("/actions/caches"):
            return {"actions_caches": self.caches}
        if path == "/actions/runs?per_page=100":
            return {"workflow_runs": self.runs}
        raise AssertionError(f"Unmocked endpoint {path}")

    def delete(self, path):
        self.deleted.append(path)
        if path.startswith("/actions/runs/"):
            rid = int(path.rsplit("/", 1)[-1])
            self.runs = [r for r in self.runs if r["id"] != rid]
        if path.startswith("/actions/artifacts/"):
            aid = int(path.rsplit("/", 1)[-1])
            self.artifacts = [x for x in self.artifacts if x["id"] != aid]
        if path.startswith("/actions/caches/"):
            cid = int(path.rsplit("/", 1)[-1])
            self.caches = [x for x in self.caches if x["id"] != cid]


def test_preservation_requires_successful_unexpired_user_exe():
    fake = FakeAPI()
    assert cleaner.find_last_usable_build(fake) == 220
    fake.artifacts[0]["expired"] = True
    with pytest.raises(RuntimeError, match="No proven successful"):
        cleaner.find_last_usable_build(fake)


def test_older_runs_are_deleted_but_latest_newer_and_active_are_untouched():
    fake = FakeAPI()
    assert cleaner.prune_runs(fake, keep=220, current=999) == 2
    assert fake.deleted == ["/actions/runs/219", "/actions/runs/218"]
    assert [r["id"] for r in fake.runs] == [222, 220]


def test_artifacts_preserve_latest_and_newer_builds():
    fake = FakeAPI()
    assert cleaner.prune_artifacts(fake, keep=220) == 1
    assert fake.deleted == ["/actions/artifacts/302"]
    assert [r["id"] for r in fake.artifacts] == [301, 303]


def test_only_ephemeral_caches_older_than_preserved_run_are_removed():
    fake = FakeAPI()
    assert cleaner.prune_caches(fake, keep=220) == 1
    assert fake.deleted == ["/actions/caches/401"]
    assert [c["id"] for c in fake.caches] == [402, 403]


def test_workflow_actions_write_without_release_or_public_artifacts():
    text = WORKFLOW.read_text(encoding="utf-8")
    assert "actions: write" in text
    assert "workflow_run:" in text
    assert "schedule:" in text
    assert "workflow_dispatch:" in text
    assert "conclusion == 'success'" in text
    assert "workflow_run.event == 'push'" in text
    assert "gh release" not in text
    assert "actions/upload-artifact" not in text
    assert "python3 scripts/prune_actions_history.py" in text


def test_batch_limits_prevent_exhausting_github_token():
    assert cleaner.MAX_ARTIFACTS + cleaner.MAX_CACHES + cleaner.MAX_RUNS <= 800
    assert "GITHUB_REPOSITORY" in SCRIPT.read_text(encoding="utf-8")
    assert "GITHUB_RUN_ID" in SCRIPT.read_text(encoding="utf-8")
