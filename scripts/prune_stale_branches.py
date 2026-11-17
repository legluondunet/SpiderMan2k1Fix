#!/usr/bin/env python3
"""Delete stale heads while retaining main and branches linked to open PRs."""
from __future__ import annotations

import json
import os
import re
import sys
import time
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen

API = "https://api.github.com"
MAIN = "main"
PER_PAGE = 100
MAX_PAGES = 100
MAX_DELETIONS = 300


class GitHub:
    def __init__(self, repository: str, token: str):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
            raise ValueError("GITHUB_REPOSITORY must be owner/repository")
        if not token:
            raise ValueError("Missing GitHub token")
        self.repository = repository
        self.token = token

    def request(self, method: str, path: str):
        if path and not path.startswith("/"):
            raise ValueError("Invalid API endpoint")
        request = Request(
            f"{API}/repos/{self.repository}{path}",
            method=method,
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {self.token}",
                "X-GitHub-Api-Version": "2022-11-28",
                "User-Agent": "SpiderMan2k1Fix-stale-branch-cleanup",
            },
        )
        try:
            with urlopen(request, timeout=45) as response:
                data = response.read()
                return json.loads(data) if data else None
        except HTTPError as error:
            detail = error.read().decode("utf-8", errors="replace")[:350]
            if method == "DELETE" and error.code == 404:
                print(f"Already deleted: {path}", flush=True)
                return None
            raise RuntimeError(f"{method} {path}: HTTP {error.code}: {detail}") from error

    def get(self, path: str):
        return self.request("GET", path)

    def delete(self, path: str):
        return self.request("DELETE", path)


def paginated(api: GitHub, endpoint: str):
    """Fetch all pages; incomplete pagination must stop cleanup."""
    items = []
    for page in range(1, MAX_PAGES + 1):
        separator = "&" if "?" in endpoint else "?"
        result = api.get(f"{endpoint}{separator}per_page={PER_PAGE}&page={page}")
        if not isinstance(result, list):
            raise RuntimeError(f"Unexpected API response from {endpoint}")
        items.extend(result)
        if len(result) < PER_PAGE:
            return items
    raise RuntimeError(f"Pagination safety cap reached: {endpoint}")


def protected_pr_branches(api: GitHub):
    """Include open PR heads and bases, including draft PRs."""
    protected = {MAIN}
    for pr in paginated(api, "/pulls?state=open"):
        head, base = pr.get("head"), pr.get("base")
        if not isinstance(head, dict) or not isinstance(base, dict):
            raise RuntimeError("Incomplete open-PR metadata")
        head_ref, base_ref = head.get("ref"), base.get("ref")
        if not head_ref or not base_ref:
            raise RuntimeError("Open PR with missing head/base ref")

        # When source repo is absent, conservatively keep same-named
        # local branches. Actual external fork heads need not be retained.
        source = head.get("repo")
        if source is None or (
            isinstance(source, dict)
            and str(source.get("full_name", "")).lower() == api.repository.lower()
        ):
            protected.add(head_ref)

        target = base.get("repo")
        if target is None or (
            isinstance(target, dict)
            and str(target.get("full_name", "")).lower() == api.repository.lower()
        ):
            protected.add(base_ref)
    return protected


def plan_deletions(branches, preserved):
    candidates, github_protected = [], 0
    for entry in branches:
        name, commit = entry.get("name"), entry.get("commit")
        if not isinstance(name, str) or not isinstance(commit, dict):
            raise RuntimeError("Malformed branch list")
        sha = commit.get("sha")
        if not isinstance(sha, str) or not re.fullmatch(r"[0-9a-f]{40}", sha):
            raise RuntimeError(f"Invalid commit SHA for branch {name}")
        if name in preserved:
            continue
        if entry.get("protected", False):
            print(f"SKIPPED GitHub-protected branch: {name}", flush=True)
            github_protected += 1
            continue
        candidates.append((name, sha))
    return candidates, github_protected


def prune(api: GitHub):
    repository = api.get("")
    if not isinstance(repository, dict):
        raise RuntimeError("Missing repository metadata")
    if repository.get("default_branch") != MAIN:
        raise RuntimeError("Default branch is not main; refusing deletion")
    if str(repository.get("full_name", "")).lower() != api.repository.lower():
        raise RuntimeError("Repository identity mismatch")

    branches = paginated(api, "/branches")
    if MAIN not in {x.get("name") for x in branches}:
        raise RuntimeError("main missing from branch inventory")
    preserved = protected_pr_branches(api)
    candidates, protected_count = plan_deletions(branches, preserved)
    if len(candidates) > MAX_DELETIONS:
        raise RuntimeError("Too many candidate branches; abort for review")

    print(
        f"Branches: {len(branches)}; open-PR/default protection: {len(preserved)}; "
        f"GitHub-protected: {protected_count}; candidates: {len(candidates)}",
        flush=True,
    )
    deleted, updated, newly_protected = [], [], []
    for name, expected_sha in candidates:
        # Re-read all open PRs immediately before EACH irreversible delete.
        # This protects PRs opened after the initial branch inventory.
        if name in protected_pr_branches(api):
            newly_protected.append(name)
            continue
        latest = api.get(f"/branches/{quote(name, safe='')}")
        if not isinstance(latest, dict) or latest.get("name") != name:
            raise RuntimeError(f"Could not verify branch identity: {name}")
        current_sha = (latest.get("commit") or {}).get("sha")
        if latest.get("protected") or current_sha != expected_sha:
            updated.append(name)
            continue

        # Only delete heads. Tags/releases are never modified.
        api.delete(f"/git/refs/heads/{quote(name, safe='/')}")
        deleted.append(name)
        print(f"DELETED: {name}", flush=True)
        time.sleep(0.35)

    print(
        f"Deleted={len(deleted)}; changed/protected={len(updated)}; "
        f"new open-PR protection={len(newly_protected)}",
        flush=True,
    )
    summary = os.getenv("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as output:
            output.write(
                "## Stale branches cleanup\n\n"
                f"Branches inventoried: {len(branches)}\n\n"
                f"Branches deleted: {len(deleted)}\n\n"
                f"GitHub-protected or changed: {len(updated)+protected_count}\n\n"
                f"Newly protected by PR: {len(newly_protected)}\n\n"
                "Preserved: main, all open PR heads and bases. Tags untouched.\n"
            )
    return deleted


def main():
    api = GitHub(os.environ["GITHUB_REPOSITORY"], os.environ["GH_TOKEN"])
    prune(api)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"BRANCH CLEANUP ABORTED: {exc}", file=sys.stderr)
        sys.exit(1)
