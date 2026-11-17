#!/usr/bin/env python3
"""Prune old Actions runs/artifacts and ephemeral caches, preserving the latest EXE.

GitHub has no bulk-delete endpoint for workflow runs. This script deletes
bounded batches, dispatching follow-up runs to finish the old history without
repeated manual clicks. Hourly scheduling remains a rate-limit fallback.
It never deletes the latest successful unified build, this cleanup run,
unfinished runs, or newer unified builds that may have just completed.
Later-running legacy CI is not protected merely because its ID is higher.
"""
import json
import os
import re
import sys
import time
from urllib.error import HTTPError
from urllib.request import Request, urlopen

BUILD_WORKFLOW = "spiderman2k1fix-unified-x86.yml"
MAX_ARTIFACTS = 120
MAX_CACHES = 50
MAX_RUNS = 550
CLEANUP_WORKFLOW = "spiderman2k1fix-actions-cleanup.yml"
CLEANUP_NAME = "Prune old SpiderMan2k1Fix Actions history"
CONTINUE_RATE_REMAINING = 160


class RateLimitPause(Exception):
    pass


class GitHub:
    def __init__(self, repo, token):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
            raise ValueError("invalid repository")
        self.repo, self.token, self.remaining = repo, token, None

    def request(self, method, path, payload=None):
        if self.remaining is not None and self.remaining < 120:
            raise RateLimitPause(f"Only {self.remaining} API requests left")
        if not path.startswith("/"):
            raise ValueError("invalid API endpoint")
        req = Request(
            f"https://api.github.com/repos/{self.repo}{path}",
            method=method,
            data=json.dumps(payload).encode("utf-8") if payload is not None else None,
            headers={
                "Accept": "application/vnd.github+json",
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self.token}",
                "X-GitHub-Api-Version": "2022-11-28",
                "User-Agent": "SpiderMan2k1Fix-storage-pruner",
            },
        )
        try:
            with urlopen(req, timeout=45) as response:
                remaining = response.headers.get("X-RateLimit-Remaining")
                if remaining is not None:
                    self.remaining = int(remaining)
                data = response.read()
                result = json.loads(data) if data else None
        except HTTPError as exc:
            remaining = exc.headers.get("X-RateLimit-Remaining")
            if remaining is not None:
                self.remaining = int(remaining)
            detail = exc.read().decode("utf-8", "replace")[:500]
            if method == "DELETE" and exc.code == 404:
                print(f"Already gone: {path}", flush=True)
                result = None
            elif exc.code in (403, 429) and (
                "rate limit" in detail.lower()
                or "secondary" in detail.lower()
                or exc.code == 429
                or self.remaining == 0
            ):
                raise RateLimitPause(f"GitHub rate limit: {exc.code} {detail}") from exc
            else:
                raise RuntimeError(f"{method} {path}: HTTP {exc.code}: {detail}") from exc
        if method == "DELETE":
            time.sleep(0.45)
        return result

    def get(self, path):
        return self.request("GET", path)

    def delete(self, path):
        return self.request("DELETE", path)

    def post(self, path, payload):
        return self.request("POST", path, payload)


def find_last_usable_build(api):
    # If no main-branch successful run still has SpiderMan2k1Fix.exe,
    # fail closed: do not delete ANYTHING.
    data = api.get(
        f"/actions/workflows/{BUILD_WORKFLOW}/runs"
        "?branch=main&status=success&per_page=25"
    )
    for run in data.get("workflow_runs", []):
        if run.get("event") != "push" or run.get("status") != "completed":
            continue
        rid = int(run["id"])
        artifacts = api.get(f"/actions/runs/{rid}/artifacts?per_page=100")
        if any(
            x.get("name") == "SpiderMan2k1Fix" and not x.get("expired", False)
            for x in artifacts.get("artifacts", [])
        ):
            return rid
    raise RuntimeError("No proven successful unified main build with a usable EXE")


def prune_artifacts(api, keep):
    deleted = 0
    while deleted < MAX_ARTIFACTS:
        page = api.get("/actions/artifacts?per_page=100").get("artifacts", [])
        eligible = [
            item for item in page
            if item.get("workflow_run")
            and item["workflow_run"].get("id")
            and int(item["workflow_run"]["id"]) < keep
        ]
        if not eligible:
            break
        for item in eligible[:MAX_ARTIFACTS - deleted]:
            api.delete(f"/actions/artifacts/{int(item['id'])}")
            deleted += 1
    return deleted


def prune_caches(api, keep):
    deleted = 0
    while deleted < MAX_CACHES:
        page = api.get("/actions/caches?per_page=100").get("actions_caches", [])
        eligible = []
        for item in page:
            match = re.fullmatch(
                r"spider-unified-internal-(\d+)-\d+", item.get("key", "")
            )
            if match and int(match.group(1)) < keep:
                eligible.append(item)
        if not eligible:
            break
        for item in eligible[:MAX_CACHES - deleted]:
            api.delete(f"/actions/caches/{int(item['id'])}")
            deleted += 1
    return deleted


def prune_runs(api, keep, current):
    deleted = 0
    while deleted < MAX_RUNS:
        # Always page ONE; as entries disappear, older ones slide into view.
        # No 1,000-run search window limitation.
        page = api.get("/actions/runs?per_page=100").get("workflow_runs", [])
        eligible = [
            x for x in page
            if x.get("status") == "completed"
            and int(x["id"]) != keep
            and int(x["id"]) != current
            # A newer compatibility workflow must not survive just because
            # its run ID exceeds the latest successful unified compilation.
            # Guard newer unified runs against races with another push.
            and (
                int(x["id"]) < keep
                or x.get("name") != "SpiderMan2k1Fix x86 (unified development)"
            )
        ]
        if not eligible:
            break
        for item in eligible[:MAX_RUNS - deleted]:
            api.delete(f"/actions/runs/{int(item['id'])}")
            deleted += 1
    return deleted



def needs_cleanup(run, keep, current):
    """Only completed runs other than the proven build and this cleaner."""
    return (
        run.get("status") == "completed"
        and int(run["id"]) not in (keep, current)
        and (
            int(run["id"]) < keep
            or run.get("name") != "SpiderMan2k1Fix x86 (unified development)"
        )
    )


def queue_next_batch(api, keep, current, counts):
    """Dispatch another bounded batch, without duplicates or API overuse."""
    if api.remaining is not None and api.remaining < CONTINUE_RATE_REMAINING:
        print("API quota too low to self-dispatch; hourly fallback enabled", flush=True)
        return False
    page = api.get("/actions/runs?per_page=100").get("workflow_runs", [])
    for run in page:
        if (
            run.get("name") == CLEANUP_NAME
            and int(run["id"]) != current
            and run.get("status") != "completed"
        ):
            print("Another cleanup already queued/running; no duplicate dispatch", flush=True)
            return False
    pending = any(needs_cleanup(run, keep, current) for run in page)
    bounded = (
        counts["artifacts"] >= MAX_ARTIFACTS
        or counts["caches"] >= MAX_CACHES
        or counts["runs"] >= MAX_RUNS
    )
    if not pending and not bounded:
        print("No remaining completed runs in recent page; stop chaining", flush=True)
        return False
    if find_last_usable_build(api) != keep:
        print("Latest build changed; its own successful-build event handles cleanup", flush=True)
        return False
    api.post(f"/actions/workflows/{CLEANUP_WORKFLOW}/dispatches", {"ref": "main"})
    print("Dispatched next cleanup batch on main (workflow_dispatch)", flush=True)
    return True

def main():
    api = GitHub(os.environ["GITHUB_REPOSITORY"], os.environ["GH_TOKEN"])
    current = int(os.environ["GITHUB_RUN_ID"])
    keep = find_last_usable_build(api)
    if keep == current:
        raise RuntimeError("Refusing to clean from the compilation being preserved")
    print(f"Preserving last successful EXE run {keep}; current cleanup {current}", flush=True)
    counts = dict(artifacts=0, caches=0, runs=0)
    try:
        counts["artifacts"] = prune_artifacts(api, keep)
        counts["caches"] = prune_caches(api, keep)
        if find_last_usable_build(api) == keep:
            counts["runs"] = prune_runs(api, keep, current)
        else:
            print("Newer successful compilation found; run history pruning deferred", flush=True)
    except RateLimitPause as exc:
        print(f"Rate-limited batch ended safely: {exc}", flush=True)
    print(f"Removed this batch: {json.dumps(counts)}", flush=True)
    chained = False
    try:
        chained = queue_next_batch(api, keep, current, counts)
    except RateLimitPause as exc:
        print(f"Self-dispatch deferred by API rate limit: {exc}", flush=True)
    summary = os.getenv("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as stream:
            stream.write(
                f"## GitHub Actions storage cleanup\n\n"
                f"Preserved successful unified build: **run {keep}**.\n\n"
                f"Deleted in this batch: {counts['artifacts']} artifacts, "
                f"{counts['caches']} ephemeral caches, "
                f"{counts['runs']} old completed workflow runs.\n\n"
                "Further batches may be needed due to GitHub API limits. "
                "The current maintenance run remains visible while active.\n"
                f"Follow-up batch dispatched: **{chained}**.\n"
            )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"ABORTED: {exc}", file=sys.stderr)
        sys.exit(1)
