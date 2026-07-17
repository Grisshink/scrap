import re

def main() -> None:
    """Generate release notes for the latest GitHub release."""

    with open("CHANGELOG.md", encoding="utf-8") as f:
        changelog = f.read()

    start = re.search(r"^## ", changelog, flags=re.MULTILINE).start()
    end = re.search(r"^# ", changelog[start:], flags=re.MULTILINE).start() + start

    latest_changes = changelog[start:end].strip()

    release_notes = f"""# Changelog

{latest_changes}
"""
    
    print(release_notes)

if __name__ == "__main__":
    main()