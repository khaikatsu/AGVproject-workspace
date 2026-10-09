"""Read-only consistency checks for the AGV documentation workspace."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit
from zipfile import ZipFile
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SKILLS = ("agv-web", "agv-server", "agv-algorithms", "agv-simulation", "agv-network", "agv-firmware")


def main() -> int:
    errors: list[str] = []
    markdown = [ROOT / "README.md", ROOT / "AGENTS.md", ROOT / "THIRD_PARTY_REFERENCES.md"]
    for folder in ("docs", ".agents/skills", "contracts", "firmware", "simulation", "tests"):
        markdown.extend((ROOT / folder).rglob("*.md"))
    links = 0
    for file in markdown:
        if not file.is_file():
            errors.append(f"Missing {file.relative_to(ROOT)}")
            continue
        content = file.read_text(encoding="utf-8")
        for target in re.findall(r"\]\(([^)]+)\)", content):
            target = target.strip().strip("<>")
            parsed = urlsplit(target)
            if parsed.scheme or target.startswith("#"):
                continue
            path = unquote(parsed.path)
            if path and not (file.parent / path).exists():
                errors.append(f"Broken link in {file.relative_to(ROOT)}: {target}")
            links += 1

    for skill in SKILLS:
        folder = ROOT / ".agents" / "skills" / skill
        content = (folder / "SKILL.md").read_text(encoding="utf-8")
        if not content.startswith("---\n") or f"name: {skill}\n" not in content:
            errors.append(f"Invalid skill header: {skill}")
        ui = folder / "agents" / "openai.yaml"
        if not ui.exists() or f"${skill}" not in ui.read_text(encoding="utf-8"):
            errors.append(f"Missing invocation metadata: {skill}")

    json_files = list((ROOT / "contracts").rglob("*.json")) + [ROOT / "AGVproject.code-workspace"]
    for file in json_files:
        try:
            json.loads(file.read_text(encoding="utf-8"))
        except (ValueError, OSError) as exc:
            errors.append(f"Invalid JSON {file.relative_to(ROOT)}: {exc}")

    try:
        import jsonschema
    except ImportError:
        print("JSON Schema validation unavailable; install jsonschema to check fixtures.")
    else:
        schema = json.loads((ROOT / "contracts/current-command.schema.json").read_text(encoding="utf-8"))
        validator = jsonschema.Draft202012Validator(schema)
        validator.check_schema(schema)
        mutex_schema = json.loads((ROOT / "contracts/current-mutex.schema.json").read_text(encoding="utf-8"))
        mutex_validator = jsonschema.Draft202012Validator(mutex_schema)
        mutex_validator.check_schema(mutex_schema)
        fixture = json.loads((ROOT / "contracts/examples/current-mqtt.json").read_text(encoding="utf-8"))
        for message in fixture["messages"]:
            if message["topic"].endswith("/command"):
                for err in validator.iter_errors(message["payload"]):
                    errors.append(f"Invalid command fixture: {err.message}")
            if message["topic"].endswith(("/mutex_grant", "/mutex_ack", "/mutex_release")):
                for err in mutex_validator.iter_errors(message["payload"]):
                    errors.append(f"Invalid mutex fixture: {err.message}")

    for name in ("AGV_Workspace_Overview.docx", "AGV_Requirements_Template.docx"):
        file = ROOT / "docs" / "exports" / name
        if not file.is_file():
            errors.append(f"Missing Word output: {name}")
            continue
        try:
            with ZipFile(file) as archive:
                ET.fromstring(archive.read("word/document.xml"))
        except Exception as exc:
            errors.append(f"Invalid Word container {name}: {exc}")

    if errors:
        for err in errors:
            print("ERROR:", err)
        return 1
    print(f"PASS: {len(markdown)} Markdown files, {links} local links, {len(SKILLS)} skills, {len(json_files)} JSON files and 2 Word containers.")
    print("Scope: document consistency only; not application, hardware or Word layout verification.")
    return 0


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
