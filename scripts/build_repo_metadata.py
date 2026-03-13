Import("env")

import os
import re
import subprocess
from pathlib import Path


ROOT = Path(env["PROJECT_DIR"])


def parse_repo_slug(remote: str) -> str | None:
    match = re.search(r"github\.com[:/](?P<slug>[^/]+/[^/]+?)(?:\.git)?$", remote.strip(), re.IGNORECASE)
    return match.group("slug") if match else None


def resolve_repo_slug() -> str | None:
    for key in ("OMNIPAPER_GITHUB_REPOSITORY", "GITHUB_REPOSITORY"):
        candidate = os.environ.get(key, "").strip()
        if candidate:
            return candidate

    try:
        result = subprocess.run(
            ["git", "config", "--get", "remote.origin.url"],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError:
        return None

    remote = result.stdout.strip()
    return parse_repo_slug(remote) if remote else None


repo_slug = resolve_repo_slug()
if repo_slug:
    env.Append(CPPDEFINES=[("OMNIPAPER_GITHUB_REPOSITORY", env.StringifyMacro(repo_slug))])
    print(f"Configured OmniPaper OTA release repository: {repo_slug}")
else:
    print("OMNIPAPER_GITHUB_REPOSITORY not set; OTA release checks will be disabled for this build.")
