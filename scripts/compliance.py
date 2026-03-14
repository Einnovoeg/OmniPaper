#!/usr/bin/env python3
"""Compliance checks and SPDX SBOM generation for OmniPaper."""

from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CODE_PROVENANCE_PATH = ROOT / "CODE_PROVENANCE.md"
THIRD_PARTY_NOTICES_PATH = ROOT / "THIRD_PARTY_NOTICES.md"
CONTRIBUTORS_PATH = ROOT / "CONTRIBUTORS.md"
PLATFORMIO_PATH = ROOT / "platformio.ini"
USERS_PREFIX = "/" + "Users/"
VOLUMES_PREFIX = "/" + "Volumes/"
DONATION_HOST = "buy" + "meacoffee.com/"
OWNER_PLACEHOLDER = "<" + "OWNER_OR_ORG" + ">"
SANCTIONED_PERSONAL_TEXT = {
    "README.md": (
        "https://buymeacoffee.com/einnovoeg",
        "buymeacoffee.com/einnovoeg",
    ),
    ".github/FUNDING.yml": (
        "buy_me_a_coffee: einnovoeg",
    ),
}

REQUIRED_RELEASE_FILES = (
    "README.md",
    "LICENSE",
    "CHANGELOG.md",
    "DEPENDENCIES.md",
    "CODE_PROVENANCE.md",
    "CONTRIBUTORS.md",
    "THIRD_PARTY_NOTICES.md",
    "LICENSE_COMPLIANCE.md",
    "RELEASING.md",
)

# The personal-information scan targets first-party content only. Vendored
# code and submodules legitimately contain upstream names, emails, and notices.
PII_SCAN_SKIP_PREFIXES = (
    ".git/",
    ".github/workflows/generated/",
    ".pio/",
    "build/",
    "lib/LibSSH-ESP32/",
    "lib/expat/",
    "lib/miniz/",
    "lib/picojpeg/",
    "open-x4-sdk/",
)

URL_ALLOWLIST = {
    "https://docs.m5stack.com/en/arduino/m5papers3/program",
    "https://docs.m5stack.com/en/core/papers3",
}

DEPENDENCY_COMPONENT_ALIASES = {
    "WiFi": "Arduino-ESP32 Core",
    "bblanchon/ArduinoJson": "ArduinoJson",
    "links2004/WebSockets": "arduinoWebSockets",
    "ricmoo/QRCode": "QRCode",
}

SPDX_LICENSE_MAP = {
    "MIT": "MIT",
    "LGPL-2.1": "LGPL-2.1-only",
    "LGPL-2.1-or-later": "LGPL-2.1-or-later",
    "LGPL-3.0 (upstream)": "LGPL-3.0-only",
    "Public Domain": "LicenseRef-Public-Domain",
}

BINARY_EXTENSIONS = {
    ".bin",
    ".bmp",
    ".elf",
    ".epf",
    ".gif",
    ".gz",
    ".ico",
    ".jpeg",
    ".jpg",
    ".map",
    ".otf",
    ".pdf",
    ".png",
    ".pyc",
    ".ttf",
    ".u8g2",
    ".woff",
    ".woff2",
    ".zip",
}


@dataclass(frozen=True)
class Component:
    name: str
    url: str
    license_name: str
    usage: str
    local_path: str

    @property
    def normalized_url(self) -> str:
        return normalize_url(self.url)

    @property
    def requires_contributor_graph(self) -> bool:
        return self.url.startswith("https://github.com/")

    @property
    def include_in_sbom(self) -> bool:
        return self.license_name != "Documentation"

    @property
    def spdx_license(self) -> str:
        return SPDX_LICENSE_MAP.get(self.license_name, "NOASSERTION")


def try_get_origin_owner() -> str | None:
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
    if not remote:
        return None

    match = re.search(r"github\.com[:/](?P<owner>[^/]+)/[^/]+(?:\.git)?$", remote, re.IGNORECASE)
    return match.group("owner") if match else None


def iter_blocked_identifiers() -> list[str]:
    blocked = {
        OWNER_PLACEHOLDER,
    }

    # Avoid hardcoding any current maintainer identity in the repository. CI
    # can supply the owner name through GitHub Actions, while local runs can
    # infer it from the configured origin remote.
    owner_from_env = os.environ.get("GITHUB_REPOSITORY_OWNER", "").strip()
    if owner_from_env:
        blocked.add(owner_from_env)

    owner_from_remote = try_get_origin_owner()
    if owner_from_remote:
        blocked.add(owner_from_remote)

    extra_identifiers = os.environ.get("OMNIPAPER_BLOCKED_IDENTIFIERS", "")
    for identifier in extra_identifiers.split(","):
        identifier = identifier.strip()
        if identifier:
            blocked.add(identifier)

    return sorted(blocked)


def build_pii_patterns() -> list[tuple[re.Pattern[str], str]]:
    patterns: list[tuple[re.Pattern[str], str]] = [
        (re.compile(re.escape(USERS_PREFIX) + r"[A-Za-z0-9._-]+"), "absolute macOS home path"),
        (re.compile(re.escape(VOLUMES_PREFIX) + r"[A-Za-z0-9._ -]+"), "absolute mounted-volume path"),
        (re.compile(re.escape(DONATION_HOST), re.IGNORECASE), "personal donation URL"),
    ]

    for identifier in iter_blocked_identifiers():
        description = "repository owner placeholder" if identifier == OWNER_PLACEHOLDER else "blocked owner identifier"
        patterns.append((re.compile(r"\b" + re.escape(identifier) + r"\b", re.IGNORECASE), description))

    return patterns


def normalize_url(url: str) -> str:
    normalized = url.strip().strip("<>()[]{}.,;")
    normalized = normalized.removesuffix(".git").rstrip("/")
    return normalized.lower()


def load_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def strip_sanctioned_personal_references(relative_path: str, text: str) -> str:
    tokens = list(SANCTIONED_PERSONAL_TEXT.get(relative_path, ()))

    # The compliance checker intentionally contains the sanctioned allowlist
    # values, so strip them from the script source before running the generic
    # personal-identifier scan against this file.
    if relative_path == "scripts/compliance.py":
        tokens.append(DONATION_HOST)
        for sanctioned_tokens in SANCTIONED_PERSONAL_TEXT.values():
            tokens.extend(sanctioned_tokens)

    for token in sorted(set(tokens), key=len, reverse=True):
        text = text.replace(token, "")
    return text


def extract_urls(text: str) -> set[str]:
    urls: set[str] = set()
    for match in re.findall(r"https?://[^\s<>)|]+", text):
        urls.add(normalize_url(match))
    return urls


def parse_markdown_table(text: str, heading: str) -> list[dict[str, str]]:
    lines = text.splitlines()
    try:
        heading_index = next(index for index, line in enumerate(lines) if line.strip() == heading)
    except StopIteration as exc:
        raise ValueError(f"Unable to find heading: {heading}") from exc

    table_lines: list[str] = []
    seen_table = False
    for line in lines[heading_index + 1 :]:
        if line.startswith("|"):
            table_lines.append(line)
            seen_table = True
            continue
        if seen_table:
            break

    if len(table_lines) < 2:
        raise ValueError(f"No markdown table found after heading: {heading}")

    headers = [cell.strip() for cell in table_lines[0].strip("|").split("|")]
    rows: list[dict[str, str]] = []
    for row_line in table_lines[2:]:
        cells = [cell.strip() for cell in row_line.strip("|").split("|")]
        if len(cells) != len(headers):
            raise ValueError(f"Malformed markdown table row under {heading}: {row_line}")
        rows.append(dict(zip(headers, cells, strict=True)))
    return rows


def load_included_components() -> list[Component]:
    rows = parse_markdown_table(load_text(CODE_PROVENANCE_PATH), "## 2. Third-Party Code Included in This Firmware")
    components: list[Component] = []
    for row in rows:
        components.append(
            Component(
                name=row["Component"],
                url=row["Upstream URL"],
                license_name=row["License"],
                usage=row["How code is used here"],
                local_path=row["Local path / integration"],
            )
        )
    return components


def load_platformio_component_requirements() -> tuple[list[str], list[str]]:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.read(PLATFORMIO_PATH, encoding="utf-8")
    lib_deps_raw = parser.get("omnipaper_base", "lib_deps")

    required_component_names: list[str] = []
    required_component_urls: list[str] = []
    for line in lib_deps_raw.splitlines():
        dependency = line.strip()
        if not dependency:
            continue
        if "symlink://" in dependency:
            continue
        if dependency.startswith("http://") or dependency.startswith("https://"):
            required_component_urls.append(normalize_url(dependency))
            continue
        dependency_key = dependency.split("@", 1)[0].strip()
        dependency_key = dependency_key.split("=", 1)[0].strip()
        component_name = DEPENDENCY_COMPONENT_ALIASES.get(dependency_key)
        if component_name is not None:
            required_component_names.append(component_name)
    return required_component_names, required_component_urls


def git_tracked_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=False,
    )
    entries = result.stdout.decode("utf-8").split("\0")
    return [ROOT / entry for entry in entries if entry]


def should_skip_identifier_scan(relative_path: str) -> bool:
    return any(relative_path.startswith(prefix) for prefix in PII_SCAN_SKIP_PREFIXES)


def is_probably_binary(path: Path) -> bool:
    if path.suffix.lower() in BINARY_EXTENSIONS:
        return True
    try:
        path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return True
    return False


def check_required_files(errors: list[str]) -> None:
    for relative_path in REQUIRED_RELEASE_FILES:
        if not (ROOT / relative_path).exists():
            errors.append(f"Missing required release/compliance file: {relative_path}")


def check_personal_identifiers(errors: list[str]) -> None:
    pii_patterns = build_pii_patterns()
    for path in git_tracked_files():
        relative_path = path.relative_to(ROOT).as_posix()
        if should_skip_identifier_scan(relative_path) or is_probably_binary(path):
            continue
        text = strip_sanctioned_personal_references(relative_path, load_text(path))
        for pattern, description in pii_patterns:
            match = pattern.search(text)
            if match:
                errors.append(
                    f"{relative_path}: found {description}: {match.group(0)}"
                )


def check_provenance_coverage(components: list[Component], errors: list[str]) -> None:
    provenance_text = load_text(CODE_PROVENANCE_PATH)
    notices_text = load_text(THIRD_PARTY_NOTICES_PATH)
    contributors_text = load_text(CONTRIBUTORS_PATH)

    provenance_urls = extract_urls(provenance_text)
    notices_urls = extract_urls(notices_text)
    contributors_urls = extract_urls(contributors_text)

    for url in sorted(provenance_urls - URL_ALLOWLIST):
        if url not in notices_urls and url not in contributors_urls:
            errors.append(f"Source reference missing from notices/contributors: {url}")

    included_component_names = {component.name for component in components}
    included_component_urls = {component.normalized_url for component in components}
    required_component_names, required_component_urls = load_platformio_component_requirements()

    for component_name in required_component_names:
        if component_name not in included_component_names:
            errors.append(
                f"platformio.ini dependency is not documented in CODE_PROVENANCE.md: {component_name}"
            )

    for component_url in required_component_urls:
        if component_url not in included_component_urls:
            errors.append(
                f"platformio.ini dependency URL is not documented in CODE_PROVENANCE.md: {component_url}"
            )

    for component in components:
        if component.license_name == "Documentation":
            continue

        if component.normalized_url not in notices_urls:
            errors.append(
                f"Included component missing from THIRD_PARTY_NOTICES.md: {component.name} ({component.url})"
            )

        if component.normalized_url not in contributors_urls:
            errors.append(
                f"Included component missing from CONTRIBUTORS.md: {component.name} ({component.url})"
            )

        if component.requires_contributor_graph:
            contributor_graph_url = normalize_url(f"{component.url}/graphs/contributors")
            if contributor_graph_url not in contributors_urls:
                errors.append(
                    f"Contributor graph missing from CONTRIBUTORS.md: {component.name} ({component.url}/graphs/contributors)"
                )


def check_release_documentation(errors: list[str]) -> None:
    version = load_version()
    compliance_text = load_text(ROOT / "LICENSE_COMPLIANCE.md")
    release_text = load_text(ROOT / "RELEASING.md")
    changelog_text = load_text(ROOT / "CHANGELOG.md")
    if "Release Checklist" not in compliance_text:
        errors.append("LICENSE_COMPLIANCE.md is missing the release checklist section.")
    if "scripts/compliance.py check" not in compliance_text:
        errors.append("LICENSE_COMPLIANCE.md does not document the compliance checker command.")
    if "CHANGELOG.md" not in release_text:
        errors.append("RELEASING.md does not document changelog handling.")
    if "RELEASE_NOTES.md" not in release_text:
        errors.append("RELEASING.md does not document generated release notes.")
    if not re.search(rf"^## \[{re.escape(version)}\] - \d{{4}}-\d{{2}}-\d{{2}}$", changelog_text, re.MULTILINE):
        errors.append(f"CHANGELOG.md does not contain a dated entry for the current version {version}.")


def run_checks(release_mode: bool) -> int:
    errors: list[str] = []
    check_required_files(errors)
    components = load_included_components()
    check_personal_identifiers(errors)
    check_provenance_coverage(components, errors)
    if release_mode:
        check_release_documentation(errors)

    if errors:
        print("Compliance check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    scope = "release" if release_mode else "repository"
    print(f"Compliance check passed for {scope} scope.")
    print(f"Validated {len(components)} included third-party components.")
    return 0


def make_spdx_id(name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9.-]+", "-", name).strip("-")
    return f"SPDXRef-{cleaned}"


def load_version() -> str:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.read(PLATFORMIO_PATH, encoding="utf-8")
    return parser.get("omnipaper", "version").strip()


def build_sbom_document(version: str, components: list[Component]) -> dict[str, object]:
    timestamp = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    namespace_seed = hashlib.sha256(f"OmniPaper-{version}".encode("utf-8")).hexdigest()
    omni_package_id = make_spdx_id("OmniPaper")

    packages = [
        {
            "name": "OmniPaper",
            "SPDXID": omni_package_id,
            "versionInfo": version,
            "downloadLocation": "NOASSERTION",
            "licenseConcluded": "MIT",
            "licenseDeclared": "MIT",
            "supplier": "Organization: OmniPaper Maintainers",
            "copyrightText": "NOASSERTION",
            "primaryPackagePurpose": "FIRMWARE",
        }
    ]
    relationships = [
        {
            "spdxElementId": "SPDXRef-DOCUMENT",
            "relationshipType": "DESCRIBES",
            "relatedSpdxElement": omni_package_id,
        }
    ]

    for component in components:
        if not component.include_in_sbom:
            continue
        component_id = make_spdx_id(component.name)
        packages.append(
            {
                "name": component.name,
                "SPDXID": component_id,
                "downloadLocation": component.url,
                "licenseConcluded": component.spdx_license,
                "licenseDeclared": component.spdx_license,
                "copyrightText": "NOASSERTION",
                "description": component.usage,
                "externalRefs": [
                    {
                        "referenceCategory": "PACKAGE-MANAGER",
                        "referenceType": "purl",
                        "referenceLocator": f"pkg:generic/{slugify(component.name)}",
                    }
                ],
            }
        )
        relationships.append(
            {
                "spdxElementId": omni_package_id,
                "relationshipType": "DEPENDS_ON",
                "relatedSpdxElement": component_id,
            }
        )

    return {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"OmniPaper-{version}",
        "documentNamespace": f"https://omnipaper.local/spdx/{namespace_seed}",
        "creationInfo": {
            "created": timestamp,
            "creators": [
                "Tool: scripts/compliance.py",
                "Organization: OmniPaper Maintainers",
            ],
        },
        "packages": packages,
        "relationships": relationships,
    }


def slugify(value: str) -> str:
    return re.sub(r"[^a-z0-9.+-]+", "-", value.lower()).strip("-")


def write_sbom(output_path: Path) -> int:
    version = load_version()
    components = load_included_components()
    document = build_sbom_document(version, components)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wrote SPDX SBOM to {output_path.relative_to(ROOT)}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    check_parser = subparsers.add_parser("check", help="run repository compliance checks")
    check_parser.add_argument(
        "--release",
        action="store_true",
        help="enable release-specific validation rules",
    )

    sbom_parser = subparsers.add_parser("sbom", help="generate an SPDX 2.3 SBOM")
    sbom_parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "build" / "omnipaper.spdx.json",
        help="output path for the generated SPDX JSON",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.command == "check":
        return run_checks(release_mode=args.release)
    if args.command == "sbom":
        output_path = args.output
        if not output_path.is_absolute():
            output_path = ROOT / output_path
        return write_sbom(output_path)
    parser.error(f"Unknown command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
