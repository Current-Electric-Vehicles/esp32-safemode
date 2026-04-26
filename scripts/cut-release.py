#!/usr/bin/env python3
"""Cut a release: merge develop into main, wait for release-please PR, merge it, sync back."""

import subprocess
import sys
import time


def run(cmd: str, check: bool = True) -> str:
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if check and result.returncode != 0:
        print(f"ERROR: {cmd}", file=sys.stderr)
        if result.stderr:
            print(result.stderr.strip(), file=sys.stderr)
        sys.exit(1)
    return result.stdout.strip()


def confirm(msg: str) -> bool:
    return input(f"{msg} [y/N] ").strip().lower() == "y"


def main() -> int:
    # Preflight checks
    run("git fetch --all --prune")

    current = run("git rev-parse --abbrev-ref HEAD")
    status = run("git status --porcelain")
    if status:
        print("ERROR: Working tree is dirty. Commit or stash changes first.", file=sys.stderr)
        return 1

    # Check develop is up to date
    local_dev = run("git rev-parse develop")
    remote_dev = run("git rev-parse origin/develop")
    if local_dev != remote_dev:
        print("ERROR: Local develop is not up to date with origin. Run: git checkout develop && git pull", file=sys.stderr)
        return 1

    # Show what's being released
    new_commits = run("git log --oneline origin/main..origin/develop")
    if not new_commits:
        print("Nothing to release — develop and main are identical.")
        return 0

    print("Commits to release:")
    print(new_commits)
    print()

    if not confirm("Merge develop into main and cut a release?"):
        print("Aborted.")
        return 0

    # Step 1: Merge develop into main
    print("\n=> Merging develop into main...")
    run("git checkout main")
    run("git pull origin main")
    run("git merge develop --no-edit")
    run("git push origin main")
    print("   Pushed to main.")

    # Step 2: Wait for release-please to create/update the PR
    print("\n=> Waiting for release-please PR...")
    pr_number = None
    for i in range(30):
        time.sleep(10)
        result = run(
            'gh pr list --base main --label "autorelease: pending" --json number,title --jq ".[0]"',
            check=False,
        )
        if result and result != "null" and result != "":
            import json
            pr = json.loads(result)
            pr_number = pr["number"]
            print(f"   Found PR #{pr_number}: {pr['title']}")
            break
        # Also check without label filter
        result = run(
            'gh pr list --base main --head release-please--branches--main --json number,title --jq ".[0]"',
            check=False,
        )
        if result and result != "null" and result != "":
            import json
            pr = json.loads(result)
            pr_number = pr["number"]
            print(f"   Found PR #{pr_number}: {pr['title']}")
            break
        sys.stdout.write(".")
        sys.stdout.flush()

    if pr_number is None:
        print("\nERROR: Timed out waiting for release-please PR.", file=sys.stderr)
        print("Check https://github.com/Current-Electric-Vehicles/esp32-safemode/actions", file=sys.stderr)
        # Still switch back to original branch
        run(f"git checkout {current}")
        return 1

    # Step 3: Wait for CI to pass on the release PR
    print("\n=> Waiting for CI checks on the release PR...")
    for i in range(60):
        time.sleep(10)
        checks = run(f"gh pr checks {pr_number} --json name,state --jq '[.[].state] | unique'", check=False)
        if '"SUCCESS"' in checks and '"PENDING"' not in checks and '"QUEUED"' not in checks:
            print("   CI passed.")
            break
        if '"FAILURE"' in checks:
            print(f"\nWARNING: CI has failures on PR #{pr_number}.", file=sys.stderr)
            if not confirm("Merge anyway?"):
                run(f"git checkout {current}")
                return 1
            break
        sys.stdout.write(".")
        sys.stdout.flush()

    # Step 4: Merge the release PR
    if not confirm(f"\nMerge release PR #{pr_number}?"):
        print("Aborted. PR is open — merge manually when ready.")
        run(f"git checkout {current}")
        return 0

    print(f"\n=> Merging PR #{pr_number}...")
    run(f"gh pr merge {pr_number} --merge --delete-branch")
    print("   Release PR merged. GitHub Release will be created by CI.")

    # Step 5: Sync main back into develop
    print("\n=> Syncing main back into develop...")
    run("git checkout main")
    run("git pull origin main")
    run("git checkout develop")
    run("git merge main --no-edit")
    run("git push origin develop")
    print("   Develop is up to date.")

    # Switch back to original branch
    run(f"git checkout {current}")

    # Show the release
    version = run("cat version.txt")
    print(f"\n=> Release v{version} is being built. Check:")
    print(f"   https://github.com/Current-Electric-Vehicles/esp32-safemode/releases")

    return 0


if __name__ == "__main__":
    sys.exit(main())
