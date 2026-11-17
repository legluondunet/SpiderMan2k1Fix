#!/usr/bin/env python3
"""Prune old Actions runs/artifacts and ephemeral caches, preserving the latest EXE.

GitHub has no bulk-delete endpoint for workflow runs. This script deletes
bounded batches so scheduled invocations can eventually clear long history.
It never deletes the latest successful unified build, this cleanup run,
unfinished runs, or runs newer than the protected build.
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


class RateLimitPause(Exception):
    pass


class GitHub:
    def __init__(self, repo, token):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
            raise ValueError("invalid repository")
        self.repo, self.token, self.remaining = repo, token, None

    def request(self, method, path):
        if self.remaining is not None and self.remaining < 120:
            raise RateLimitPause(f"Only {self.remaining} API requests left")
        if not path.startswith("/"):
            raise ValueError("invalid API endpoint")
        req = Request(
            f"https://api.github.com/repos/{self.repo}{path}",
            method=method,
            headers={
                "Accept": "application/vnd.github+json",
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
            and int(x["id"]) < keep
            and int(x["id"]) != current
        ]
        if not eligible:
            break
        for item in eligible[:MAX_RUNS - deleted]:
            api.delete(f"/actions/runs/{int(item['id'])}")
            deleted += 1
    return deleted


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
        counts["runs"] = prune_runs(api, keep, current)
    except RateLimitPause as exc:
        print(f"Rate-limited batch ended safely: {exc}", flush=True)
    print(f"Removed this batch: {json.dumps(counts)}", flush=True)
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
            )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"ABORTED: {exc}", file=sys.stderr)
        sys.exit(1)
