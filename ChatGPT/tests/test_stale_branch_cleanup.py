"""Branch cleanup must preserve main, open PR refs and changing branches."""
import importlib.util
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlsplit

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/prune_stale_branches.py"
WORKFLOW = ROOT / ".github/workflows/spiderman2k1fix-branches-cleanup.yml"
spec = importlib.util.spec_from_file_location("spider_branch_prune", SCRIPT)
cleanup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cleanup)

REPO = "legluondunet/SpiderMan2k1Fix"
SHA = "a" * 40


def branch(name, sha=SHA, protected=False):
    return {"name": name, "commit": {"sha": sha}, "protected": protected}


def pr(head, base="main", source=REPO, draft=False):
    return {
        "head": {"ref": head, "repo": {"full_name": source}},
        "base": {"ref": base, "repo": {"full_name": REPO}},
        "draft": draft,
    }


class FakeAPI:
    repository = REPO

    def __init__(self, branches=None, prs=None):
        self.branches = branches or [branch("main"), branch("fix/stale")]
        self.prs = prs if prs is not None else []
        self.deleted = []
        self.pr_calls = 0
        self.new_open_pr_on_second_check = None
        self.moved_branch = None

    def get(self, path):
        if path == "":
            return {"default_branch": "main", "full_name": REPO}
        if path.startswith("/pulls?"):
            self.pr_calls += 1
            prs = self.prs
            if self.pr_calls >= 2 and self.new_open_pr_on_second_check:
                prs = prs + [self.new_open_pr_on_second_check]
            return self._page(prs, path)
        if path.startswith("/branches?"):
            return self._page(self.branches, path)
        if path.startswith("/branches/"):
            name = unquote(path[len("/branches/"):])
            result = next(x for x in self.branches if x["name"] == name)
            if self.moved_branch == name:
                result = branch(name, sha="b" * 40)
            return result
        raise AssertionError(f"Unexpected API: {path}")

    @staticmethod
    def _page(items, path):
        params = parse_qs(urlsplit(path).query)
        page = int(params["page"][0])
        size = int(params["per_page"][0])
        return items[(page-1)*size:page*size]

    def delete(self, path):
        assert path.startswith("/git/refs/heads/")
        name = unquote(path[len("/git/refs/heads/"):])
        self.deleted.append(name)
        self.branches = [x for x in self.branches if x["name"] != name]


def test_cleanup_keeps_main_and_both_sides_of_open_pr(monkeypatch):
    monkeypatch.setattr(cleanup.time, "sleep", lambda _: None)
    api = FakeAPI(
        branches=[
            branch("main"), branch("feature/draft"),
            branch("feature/target"), branch("fix/stale"),
            branch("fix/another"), branch("release/protected", protected=True),
        ],
        prs=[pr("feature/draft", "feature/target", draft=True)],
    )
    assert cleanup.prune(api) == ["fix/stale", "fix/another"]
    assert set(x["name"] for x in api.branches) == {
        "main", "feature/draft", "feature/target", "release/protected"
    }


def test_fork_pr_does_not_preserve_unrelated_local_branch(monkeypatch):
    monkeypatch.setattr(cleanup.time, "sleep", lambda _: None)
    api = FakeAPI(
        branches=[branch("main"), branch("patch-1")],
        prs=[pr("patch-1", source="some-contributor/fork")],
    )
    assert cleanup.prune(api) == ["patch-1"]


def test_pr_opened_after_initial_inventory_blocks_deletion(monkeypatch):
    monkeypatch.setattr(cleanup.time, "sleep", lambda _: None)
    api = FakeAPI()
    api.new_open_pr_on_second_check = pr("fix/stale")
    assert cleanup.prune(api) == []
    assert api.deleted == []


def test_branch_moved_after_inventory_is_preserved(monkeypatch):
    monkeypatch.setattr(cleanup.time, "sleep", lambda _: None)
    api = FakeAPI()
    api.moved_branch = "fix/stale"
    assert cleanup.prune(api) == []
    assert api.deleted == []


def test_malformed_open_pr_fails_closed(monkeypatch):
    monkeypatch.setattr(cleanup.time, "sleep", lambda _: None)
    api = FakeAPI(prs=[{"head": {}, "base": {"ref": "main"}}])
    with pytest.raises(RuntimeError, match="missing head/base"):
        cleanup.prune(api)
    assert api.deleted == []


def test_all_pages_are_read_before_deletion():
    api = FakeAPI(branches=[branch("main")] + [
        branch(f"test/branch-{i}") for i in range(112)
    ])
    assert len(cleanup.paginated(api, "/branches")) == 113


def test_default_branch_guard_and_upper_bound(monkeypatch):
    monkeypatch.setattr(cleanup.time, "sleep", lambda _: None)
    api = FakeAPI(branches=[branch("main")] + [
        branch(f"test/branch-{i}") for i in range(cleanup.MAX_DELETIONS + 1)
    ])
    with pytest.raises(RuntimeError, match="Too many candidate"):
        cleanup.prune(api)
    assert api.deleted == []


def test_workflow_is_trusted_has_write_permissions_and_no_release_steps():
    flow = WORKFLOW.read_text(encoding="utf-8")
    assert "pull_request:" in flow and "types: [closed]" in flow
    assert "schedule:" in flow and "workflow_dispatch:" in flow
    assert "contents: write" in flow and "pull-requests: read" in flow
    assert "ref: main" in flow
    assert "persist-credentials: false" in flow
    assert "python3 scripts/prune_stale_branches.py" in flow
    assert "gh release" not in flow
