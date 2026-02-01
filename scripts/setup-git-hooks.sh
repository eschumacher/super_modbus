#!/bin/bash
# Setup script to install git hooks

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOKS_DIR="$REPO_ROOT/.githooks"
GIT_HOOKS_DIR="$REPO_ROOT/.git/hooks"

echo "Setting up git hooks..."

# Create .git/hooks directory if it doesn't exist
mkdir -p "$GIT_HOOKS_DIR"

# Install pre-commit hook
if [ -f "$HOOKS_DIR/pre-commit" ]; then
  chmod +x "$HOOKS_DIR/pre-commit"
  ln -sf "$HOOKS_DIR/pre-commit" "$GIT_HOOKS_DIR/pre-commit"
  echo "✓ Installed pre-commit hook"
else
  echo "✗ pre-commit hook not found"
  exit 1
fi

# Install pre-push hook
if [ -f "$HOOKS_DIR/pre-push" ]; then
  chmod +x "$HOOKS_DIR/pre-push"
  ln -sf "$HOOKS_DIR/pre-push" "$GIT_HOOKS_DIR/pre-push"
  echo "✓ Installed pre-push hook"
else
  echo "✗ pre-push hook not found"
  exit 1
fi

echo ""
echo "Git hooks installed successfully!"
echo ""
echo "The hooks will now:"
echo "  - Check formatting before each commit (pre-commit)"
echo "  - Check formatting before each push (pre-push)"
echo ""
echo "To bypass hooks (not recommended):"
echo "  git commit --no-verify"
echo "  git push --no-verify"
