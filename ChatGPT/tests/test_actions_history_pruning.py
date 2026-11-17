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
        self.dispatched = []
        self.remaining = 500
        self.runs = [
            {"id": 222, "status": "in_progress", "event": "push"},
            {"id": 221, "status": "completed", "event": "push",
             "name": "SpiderMan2k1Fix Windows x86"},
            {"id": 220, "status": "completed", "event": "push",
             "name": "SpiderMan2k1Fix x86 (unified development)"},
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

    def post(self, path, payload):
        self.dispatched.append((path, payload))

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


def test_legacy_runs_after_build_are_deleted_but_latest_and_active_are_untouched():
    fake = FakeAPI()
    assert cleaner.prune_runs(fake, keep=220, current=999) == 3
    assert fake.deleted == [
        "/actions/runs/221", "/actions/runs/219", "/actions/runs/218"
    ]
    assert [r["id"] for r in fake.runs] == [222, 220]


def test_newer_unified_build_is_held_for_next_cleanup_pass():
    fake = FakeAPI()
    fake.runs.insert(
        0, {"id": 223, "status": "completed", "event": "push",
            "name": "SpiderMan2k1Fix x86 (unified development)"}
    )
    assert cleaner.prune_runs(fake, keep=220, current=999) == 3
    assert [r["id"] for r in fake.runs] == [223, 222, 220]


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


def test_followup_requeues_when_old_completed_runs_remain():
    fake = FakeAPI()
    result = cleaner.queue_next_batch(
        fake, keep=220, current=999, counts={"runs": 3, "artifacts": 0, "caches": 0}
    )
    assert result is True
    assert fake.dispatched == [
        ("/actions/workflows/spiderman2k1fix-actions-cleanup.yml/dispatches",
         {"ref": "main"})
    ]


def test_no_requeue_after_history_fully_drained():
    fake = FakeAPI()
    fake.runs = [fake.runs[2], {"id": 999, "status": "in_progress",
                               "name": cleaner.CLEANUP_NAME}]
    counts = {"runs": 0, "artifacts": 0, "caches": 0}
    assert cleaner.queue_next_batch(fake, keep=220, current=999, counts=counts) is False
    assert fake.dispatched == []


def test_no_extra_followup_when_cleanup_is_already_queued():
    fake = FakeAPI()
    fake.runs.insert(0, {"id": 1000, "name": cleaner.CLEANUP_NAME,
                         "status": "queued"})
    assert not cleaner.queue_next_batch(
        fake, keep=220, current=999,
        counts={"runs": 550, "artifacts": 120, "caches": 0}
    )
    assert fake.dispatched == []


def test_rate_limit_stops_self_dispatch_but_existing_hourly_cron_remains():
    fake = FakeAPI()
    fake.remaining = cleaner.CONTINUE_RATE_REMAINING - 1
    assert not cleaner.queue_next_batch(
        fake, keep=220, current=999,
        counts={"runs": 550, "artifacts": 0, "caches": 0}
    )
    assert fake.dispatched == []
    workflow = WORKFLOW.read_text(encoding="utf-8")
    assert 'cron: "23 * * * *"' in workflow
    assert "actions: write" in workflow
    assert "workflow_dispatch:" in workflow


def test_followup_is_not_launched_if_newer_unified_build_appeared():
    fake = FakeAPI()
    newer = {"id": 224, "status": "completed", "event": "push",
             "name": "SpiderMan2k1Fix x86 (unified development)"}
    newer_artifact = {"id": 330, "name": "SpiderMan2k1Fix", "expired": False,
                      "workflow_run": {"id": 224}}
    fake.runs.insert(0, newer)
    fake.artifacts.insert(0, newer_artifact)
    assert not cleaner.queue_next_batch(
        fake, keep=220, current=999,
        counts={"runs": 550, "artifacts": 0, "caches": 0}
    )
    assert fake.dispatched == []
