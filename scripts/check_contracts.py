"""Validate the machine-readable MQTT contracts used by the AGV code."""

import json
from pathlib import Path

from jsonschema import Draft202012Validator


ROOT = Path(__file__).resolve().parents[1]
CONTRACTS = ROOT / "contracts"


def main() -> int:
    documents = {}
    for path in CONTRACTS.rglob("*.json"):
        documents[path.relative_to(CONTRACTS).as_posix()] = json.loads(
            path.read_text(encoding="utf-8")
        )

    command = Draft202012Validator(documents["current-command.schema.json"])
    mutex = Draft202012Validator(documents["current-mutex.schema.json"])
    command.check_schema(command.schema)
    mutex.check_schema(mutex.schema)

    errors = []
    for message in documents["examples/current-mqtt.json"]["messages"]:
        topic = message["topic"]
        validator = command if topic.endswith("/command") else (
            mutex if topic.endswith(("/mutex_grant", "/mutex_ack", "/mutex_release")) else None
        )
        if validator is None:
            continue
        for error in validator.iter_errors(message["payload"]):
            errors.append(f"{topic}: {error.message}")

    if errors:
        for error in errors:
            print("ERROR:", error)
        return 1
    print(f"PASS: {len(documents)} contract JSON files and MQTT command/mutex fixtures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
