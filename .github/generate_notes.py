import re

with open('CHANGELOG.md') as f:
    changelog = f.read()

start = re.search('## ', changelog).start()
end   = re.search('^# ', changelog[start:], flags=re.MULTILINE).start() + start

print(changelog[start:end].strip())
